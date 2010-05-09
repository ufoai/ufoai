// bumpmap vertex shader, requires light_vs.glsl and lerp_vs.glsl

attribute vec4 TANGENT;
uniform int DYNAMICLIGHTS;

varying vec3 eyedir;
varying vec3 lightDirs[#replace r_dynamic_lights ];
varying vec3 staticLightDir;

/*
 * BumpVertex
 */
void BumpVertex(void){

	// load the tangent
	vec3 tangent = normalize(gl_NormalMatrix * Tangent.xyz );
	// compute the bitangent
	vec3 bitangent = normalize(cross(normal, tangent)) * Tangent.w;

	// transform the eye direction into tangent space
	vec3 v;
	v.x = dot(-point, tangent);
	v.y = dot(-point, bitangent);
	v.z = dot(-point, normal);

	eyedir = normalize(v);

	// transform relative light positions into tangent space
	vec3 lpos = normalize(lightpos - point);
	staticLightDir.x = dot(lpos, tangent);
	staticLightDir.y = dot(lpos, bitangent);
	staticLightDir.z = dot(lpos, normal);

	if(DYNAMICLIGHTS > 0) {
#unroll r_dynamic_lights
		if(gl_LightSource[$].position.a != 0.0) {
			lpos = gl_LightSource[$].position.rgb - point;
		} else { /* directional light source at "infinite" distance */
			lpos = normalize(gl_LightSource[$].position.rgb);
		}
		lightDirs[$].x = dot(lpos, tangent);
		lightDirs[$].y = dot(lpos, bitangent);
		lightDirs[$].z = dot(lpos, normal);
#endunroll
	}
}
