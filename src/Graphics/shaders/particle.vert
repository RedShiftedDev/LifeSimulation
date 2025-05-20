#version 410 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec4 aInstanceData; // x, y, radius, active
layout (location = 3) in vec4 aColor;        // r, g, b, padding

out vec2 TexCoord;
out vec4 Color;
out float Active;

uniform mat4 projection;

void main()
{
    // Extract position, size, and active status
    vec2 position = aInstanceData.xy;
    float radius = aInstanceData.z;
    Active = aInstanceData.w;
    
    // Scale and position the particle
    vec3 pos = vec3(aPos.x * radius + position.x, aPos.y * radius + position.y, aPos.z);
    
    gl_Position = projection * vec4(pos, 1.0);
    
    // Pass texture coordinates to fragment shader
    TexCoord = aTexCoord;
    
    // Pass color to fragment shader
    Color = vec4(aColor.rgb, 1.0);
}