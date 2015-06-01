/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "http.h"
#ifndef NO_HTTP
#include "../shared/shared.h"
#include <SDL_thread.h>

/**
 * @brief Extract the servername, the port and the path part of the given url
 * @param url The url to extract the data from
 * @param server The server target buffer
 * @param serverLength The length of the buffer
 * @param path The path target buffer
 * @param pathLength The length of the buffer
 * @param port The port
 * @return @c true if the extracting went well, @c false if an error occurred
 */
bool HTTP_ExtractComponents (const char* url, char* server, size_t serverLength, char* path, size_t pathLength, int* port)
{
	const char* proto = "http://";
	const size_t protoLength = strlen(proto);
	char buffer[1024];

	if (Q_strncasecmp(proto, url, protoLength))
		return false;

	Q_strncpyz(buffer, url, sizeof(buffer));
	char* buf = buffer;

	buf += protoLength;
	int i = 0;
	char* s;
	for (s = server; *buf != '\0' && *buf != ':' && *buf != '/';) {
		if (i >= serverLength - 1)
			return false;
		i++;
		*s++ = *buf++;
	}
	*s = '\0';

	if (*buf == ':') {
		buf++;
		if (sscanf(buf, "%d", port) != 1)
			return false;

		for (buf++; *buf != '\0' && *buf != '/'; buf++) {
		}
	} else {
		*port = 80;
	}

	Q_strncpyz(path, buf, pathLength);

	return true;
}

/**
 * @brief libcurl callback to update header info.
 */
size_t HTTP_Header (void* ptr, size_t size, size_t nmemb, void* stream)
{
	char headerBuff[1024];
	const size_t bytes = size * nmemb;
	size_t len;

	if (bytes <= 16)
		return bytes;

	if (bytes < sizeof(headerBuff))
		len = bytes + 1;
	else
		len = sizeof(headerBuff);

	Q_strncpyz(headerBuff, (const char*)ptr, len);

	if (!Q_strncasecmp(headerBuff, "Content-Length: ", 16)) {
		dlhandle_t* dl = (dlhandle_t*)stream;
		if (dl->file)
			dl->fileSize = strtoul(headerBuff + 16, nullptr, 10);
	}

	return bytes;
}

/**
 * @brief libcurl callback for HTTP_GetURL
 */
size_t HTTP_Recv (void* ptr, size_t size, size_t nmemb, void* stream)
{
	const size_t bytes = size * nmemb;
	dlhandle_t* dl = (dlhandle_t*)stream;

	if (!dl->fileSize) {
		dl->fileSize = bytes > 131072 ? bytes : 131072;
		dl->tempBuffer = Mem_AllocTypeN(char, dl->fileSize);
	} else if (dl->position + bytes >= dl->fileSize - 1) {
		char* tmp = dl->tempBuffer;
		dl->tempBuffer = Mem_AllocTypeN(char, dl->fileSize * 2);
		memcpy(dl->tempBuffer, tmp, dl->fileSize);
		Mem_Free(tmp);
		dl->fileSize *= 2;
	}

	memcpy(dl->tempBuffer + dl->position, ptr, bytes);
	dl->position += bytes;
	dl->tempBuffer[dl->position] = 0;

	return bytes;
}

/**
 * @brief Converts the hostname into an ip to work around a bug in libcurl (resp. the resolver) that
 * uses alarm for timeouts (this is in conflict with our signal handlers and longjmp environment)
 * @param[in] url The url to convert
 * @param[out] buf The resolved url or empty if an error occurred
 * @param[in] size The size of the target buffer
 */
static void HTTP_ResolvURL (const char* url, char* buf, size_t size)
{
	char server[512];
	char ipServer[MAX_VAR];
	int port;
	char uriPath[512];

	buf[0] = '\0';

	if (!HTTP_ExtractComponents(url, server, sizeof(server), uriPath, sizeof(uriPath), &port))
		Com_Error(ERR_DROP, "invalid url given: %s", url);

	NET_ResolvNode(server, ipServer, sizeof(ipServer));
	if (ipServer[0] != '\0')
		Com_sprintf(buf, size, "http://%s:%i%s", ipServer, port, uriPath);
}

/**
 * @brief Gets a specific url
 * @note Make sure, that you free the string that is returned by this function
 */
static bool HTTP_GetURLInternal (dlhandle_t& dl, const char* url, FILE* file, const char* postfields)
{
	if (Q_strnull(url)) {
		Com_Printf("invalid url given\n");
		return false;
	}

	char buf[576];
	HTTP_ResolvURL(url, buf, sizeof(buf));
	if (buf[0] == '\0') {
		Com_Printf("could not resolve '%s'\n", url);
		return false;
	}
	Q_strncpyz(dl.URL, url, sizeof(dl.URL));

	dl.curl = curl_easy_init();
	curl_easy_setopt(dl.curl, CURLOPT_CONNECTTIMEOUT, http_timeout->integer);
	curl_easy_setopt(dl.curl, CURLOPT_TIMEOUT, http_timeout->integer);
	curl_easy_setopt(dl.curl, CURLOPT_ENCODING, "");
	curl_easy_setopt(dl.curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(dl.curl, CURLOPT_FAILONERROR, 1);
	if (file) {
		curl_easy_setopt(dl.curl, CURLOPT_WRITEDATA, file);
		curl_easy_setopt(dl.curl, CURLOPT_WRITEFUNCTION, nullptr);
	} else {
		curl_easy_setopt(dl.curl, CURLOPT_WRITEDATA, &dl);
		curl_easy_setopt(dl.curl, CURLOPT_WRITEFUNCTION, HTTP_Recv);
	}
	curl_easy_setopt(dl.curl, CURLOPT_PROXY, http_proxy->string);
	curl_easy_setopt(dl.curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(dl.curl, CURLOPT_MAXREDIRS, 5);
	curl_easy_setopt(dl.curl, CURLOPT_WRITEHEADER, &dl);
	if (postfields != nullptr)
		curl_easy_setopt(dl.curl, CURLOPT_POSTFIELDS, postfields);
	curl_easy_setopt(dl.curl, CURLOPT_HEADERFUNCTION, HTTP_Header);
	curl_easy_setopt(dl.curl, CURLOPT_USERAGENT, GAME_TITLE " " UFO_VERSION);
	curl_easy_setopt(dl.curl, CURLOPT_URL, dl.URL);
	curl_easy_setopt(dl.curl, CURLOPT_NOSIGNAL, 1);

	/* get it */
	const CURLcode result = curl_easy_perform(dl.curl);
	if (result != CURLE_OK)	{
		if (result == CURLE_HTTP_RETURNED_ERROR) {
			long httpCode = 0;
			curl_easy_getinfo(dl.curl, CURLINFO_RESPONSE_CODE, &httpCode);
			Com_Printf("failed to fetch '%s': %s (%i)\n", url, curl_easy_strerror(result), (int)httpCode);
		} else {
			Com_Printf("failed to fetch '%s': %s\n", url, curl_easy_strerror(result));
		}
		curl_easy_cleanup(dl.curl);
		return false;
	}

	/* clean up */
	curl_easy_cleanup(dl.curl);

	return true;
}

bool HTTP_PutFile (const char* formName, const char* fileName, const char* url, const upparam_t* params)
{
	if (Q_strnull(url)) {
		Com_Printf("no upload url given\n");
		return false;
	}

	if (Q_strnull(fileName)) {
		Com_Printf("no upload fileName given\n");
		return false;
	}

	if (Q_strnull(formName)) {
		Com_Printf("no upload formName given\n");
		return false;
	}

	char buf[576];
	HTTP_ResolvURL(url, buf, sizeof(buf));
	if (buf[0] == '\0') {
		Com_Printf("could not resolve '%s'\n", url);
		return false;
	}

	CURL* curl = curl_easy_init();
	if (curl == nullptr) {
		Com_Printf("could not init curl\n");
		return false;
	}

	struct curl_httppost* post = nullptr;
	struct curl_httppost* last = nullptr;
	while (params) {
		curl_formadd(&post, &last, CURLFORM_PTRNAME, params->name, CURLFORM_PTRCONTENTS, params->value, CURLFORM_END);
		params = params->next;
	}

	curl_formadd(&post, &last, CURLFORM_PTRNAME, formName, CURLFORM_FILE, fileName, CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, http_timeout->integer);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, http_timeout->integer);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, GAME_TITLE " " UFO_VERSION);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	const CURLcode result = curl_easy_perform(curl);
	if (result != CURLE_OK)	{
		if (result == CURLE_HTTP_RETURNED_ERROR) {
			long httpCode = 0;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
			Com_Printf("failed to upload file '%s': %s (%i)\n", fileName, curl_easy_strerror(result), (int)httpCode);
		} else {
			Com_Printf("failed to upload file '%s': %s\n", fileName, curl_easy_strerror(result));
		}
		curl_easy_cleanup(curl);
		return false;
	}

	curl_easy_cleanup(curl);
	return true;
}

/**
 * @brief Downloads the given @c url into the given @c file.
 * @param[in] url The url to fetch
 * @param[in] file The file to write the result into
 * @param[in] postfields Some potential POST data in the form
 */
bool HTTP_GetToFile (const char* url, FILE* file, const char* postfields)
{
	if (!file)
		return false;
	dlhandle_t dl;
	OBJZERO(dl);

	return HTTP_GetURLInternal(dl, url, file, postfields);
}

/**
 * @brief This function converts the given url to an URL encoded string.
 * All input characters that are not a-z, A-Z, 0-9, '-', '.', '_' or '~' are converted to their "URL escaped" version
 * (%NN where NN is a two-digit hexadecimal number).
 * @return @c true if the conversion was successful, @c false if it failed or the target buffer was too small.
 */
bool HTTP_Encode (const char* url, char* out, size_t outLength)
{
	CURL* curl = curl_easy_init();
	char* encoded = curl_easy_escape(curl, url, 0);
	if (encoded == nullptr) {
		curl_easy_cleanup(curl);
		return false;
	}
	Q_strncpyz(out, encoded, outLength);
	const bool success = strlen(encoded) < outLength;
	curl_free(encoded);
	curl_easy_cleanup(curl);
	return success;
}

/**
 * @brief Downloads the given @c url and return the data to the callback (optional)
 * @param[in] url The url to fetch
 * @param[in] callback The callback to give the data to. Might also be @c NULL
 * @param[in] userdata The userdata that is given to the callback
 * @param[in] postfields Some potential POST data
 */
bool HTTP_GetURL (const char* url, http_callback_t callback, void* userdata, const char* postfields)
{
	dlhandle_t dl;
	OBJZERO(dl);

	if (!HTTP_GetURLInternal(dl, url, nullptr, postfields)) {
		Mem_Free(dl.tempBuffer);
		dl.tempBuffer = nullptr;
		return false;
	}

	if (callback != nullptr)
		callback(dl.tempBuffer, userdata);

	Mem_Free(dl.tempBuffer);
	dl.tempBuffer = nullptr;
	return true;
}

/**
 * @brief UFO is exiting or we're changing servers. Clean up.
 */
void HTTP_Cleanup (void)
{
	curl_global_cleanup();
}
#else
void HTTP_GetURL(const char* url, http_callback_t callback) {}
void HTTP_PutFile(const char* formName, const char* fileName, const char* url, const upparam_t* params) {}
size_t HTTP_Recv(void* ptr, size_t size, size_t nmemb, void* stream) {return 0L;}
size_t HTTP_Header(void* ptr, size_t size, size_t nmemb, void* stream) {return 0L;}
void HTTP_Cleanup(void) {}
bool HTTP_ExtractComponents(const char* url, char* server, size_t serverLength, char* path, size_t pathLength, int* port) {return false;}
#endif
