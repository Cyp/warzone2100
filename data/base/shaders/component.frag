#version 120
//#pragma debug(on)

uniform sampler2D Texture0; //diffuse
uniform sampler2D Texture1; //tcmask
uniform sampler2D Texture2; //normal map
uniform sampler2D Texture3; //spec map
uniform vec4 teamcolour;
uniform int tcmask, normalmap, specularmap;
uniform int fogEnabled;
uniform bool ecmEffect;
uniform float graphicsCycle;

varying float vertexDistance;
varying vec4 positionView; // position in view coordinates
varying vec3 t,b,n; // mapping from local surface coordinates to view coordinates

void main(void)
{
	vec4 mask, colour;
	vec4 light = gl_FrontLightModelProduct.sceneColor + (gl_LightSource[0].ambient * gl_FrontMaterial.ambient);
	vec3 N;
	vec3 L = normalize(vec3(gl_LightSource[0].position)); // directional light
	vec2 texCoord = gl_TexCoord[0].st;

	mat3 localSurface2View = mat3(t, b, n);
	// tangent normal map implementation
	if (normalmap == 1)
	{
		N = normalize(localSurface2View * (vec3(2.0, 2.0, 1.0) * (texture2D(Texture2, texCoord.st).rgb) - vec3(1.0, 1.0, 0.0)));
	}
	else //no normalmap, grab normal from tbn matrix
	{
		N = normalize(localSurface2View[2]);
	}

	float lambertTerm = dot(N, L);
	if (lambertTerm > 0.0)
	{
		vec4 matspecolor = gl_FrontMaterial.specular;
		float shininess = gl_FrontMaterial.shininess;

		if (specularmap == 1)
		{
			//shininess = texture2D(Texture3, gl_TexCoord[0].st).a;
			matspecolor = vec4(texture2D(Texture3, texCoord.st).rgb, 1.0);
		}

		light += gl_LightSource[0].diffuse * gl_FrontMaterial.diffuse * lambertTerm;
		vec3 E = normalize(vec3(-positionView));
		vec3 R = reflect(-L, N);
		float specularterm = pow(max(dot(R, E), 0.0), shininess);
		light += gl_LightSource[0].specular * matspecolor * specularterm;
	}

	// Get color from texture unit 0, merge with lighting
	colour = texture2D(Texture0, texCoord.st) * light;

	if (tcmask == 1)
	{
		// Get tcmask information from texture unit 1
		mask = texture2D(Texture1, texCoord.st);

		// Apply color using grain merge with tcmask
		gl_FragColor = (colour + (teamcolour - 0.5) * mask.a) * gl_Color;
	}
	else
	{
		gl_FragColor = colour * gl_Color;
	}

	if (ecmEffect)
	{
		gl_FragColor.a = 0.45 + 0.225 * graphicsCycle;
	}

	if (fogEnabled > 0)
	{
		// Calculate linear fog
		float fogFactor = (gl_Fog.end - vertexDistance) / (gl_Fog.end - gl_Fog.start);
		fogFactor = clamp(fogFactor, 0.0, 1.0);

		// Return fragment color
		gl_FragColor = mix(gl_Fog.color, gl_FragColor, fogFactor);
	}
}
