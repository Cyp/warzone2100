#version 120
//#pragma debug(on)

attribute vec4 tangent;

uniform float stretch;

varying float vertexDistance;
varying vec4 positionView; // position in view coordinates
varying vec3 t,b,n; // mapping from local surface coordinates to view coordinates

void main(void)
{
	n = normalize(gl_NormalMatrix * gl_Normal);
	t = normalize(gl_NormalMatrix * tangent.xyz);
	b = normalize(cross(n, t) * tangent.w);

	positionView = gl_ModelViewMatrix * gl_Vertex;

	// Pass texture coordinates to fragment shader
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;

	gl_FrontColor = gl_Color;

	// Implement building stretching to accomodate terrain
	vec4 position = gl_Vertex;
	if (position.y <= 0.0)
	{
		position.y -= stretch;
	}

	// Translate every vertex according to the Model View and Projection Matrix
	gl_Position = gl_ModelViewProjectionMatrix * position;

	// Remember vertex distance
	vertexDistance = gl_Position.z;
}
