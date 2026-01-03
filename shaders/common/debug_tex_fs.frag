#version 460 core

// ===== Fragment Inputs =====
in VS_OUT {
    vec2 uv;
} fs_in;

// ===== Fragment Outputs =====
layout(location = 0) out vec4 o_Color;

// ===== Resources =====
// binding=0 对应你在 CPU 侧绑定到 texture unit 0（或 glBindTextureUnit(0, tex)）
layout(binding = 0) uniform sampler2D u_AlbedoTex;

void main()
{
    o_Color = texture(u_AlbedoTex, fs_in.uv);
}
