#version 460 core

// ============================================================================
// 后处理全屏四边形顶点着色器
// - 使用 gl_VertexID 生成全屏三角形（无需 VBO）
// - 输出 UV 坐标用于采样屏幕纹理
// ============================================================================

// ===== Vertex Outputs =====
out VS_OUT {
    vec2 texCoord;
} vs_out;

void main()
{
    // 使用 gl_VertexID 生成全屏三角形
    // Vertex 0: (-1, -1) -> UV (0, 0)
    // Vertex 1: ( 3, -1) -> UV (2, 0)
    // Vertex 2: (-1,  3) -> UV (0, 2)
    // 这个技巧避免了需要 VBO，并且覆盖整个屏幕
    vec2 vertices[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    
    vec2 texCoords[3] = vec2[3](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );
    
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
    vs_out.texCoord = texCoords[gl_VertexID];
}
