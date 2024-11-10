#version 450 

#extension GL_EXT_fragment_shader_barycentric : require

layout(location = 0) out vec4 outColor;
layout(location = 0) in pervertexEXT vec3 inColor[];

void main() {
  vec3 color = (inColor[0].xyz * gl_BaryCoordEXT.x) + (inColor[1].xyz * gl_BaryCoordEXT.y) + (inColor[2].xyz * gl_BaryCoordEXT.z);

  // White thick border line
  {
    float d = min(min(gl_BaryCoordEXT.x, gl_BaryCoordEXT.y), gl_BaryCoordEXT.z) - 0.0625;
    color = mix(vec3(1.0, 1.0, 1.0), color, smoothstep(0.0, max(1e-6, fwidth(d) * 1.41421356), d));
  }

  // Anti-aliasing
  {
    float d = min(min(gl_BaryCoordEXT.x, gl_BaryCoordEXT.y), gl_BaryCoordEXT.z);
    color = mix(vec3(0.0, 0.0, 0.0), color, smoothstep(0.0, max(1e-6, fwidth(d) * 1.41421356), d));
  }
  
  outColor = vec4(color, 1.0);
}