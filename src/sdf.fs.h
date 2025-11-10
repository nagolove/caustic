#pragma once

const char *fs_code_sdf =
"#version 330\n"
"// Input vertex attributes (from vertex shader)\n"
"in vec2 fragTexCoord;\n"
"in vec4 fragColor;\n"
"// Input uniform values\n"
"uniform sampler2D texture0;\n"
"uniform vec4 colDiffuse;\n"
"// Output fragment color\n"
"out vec4 finalColor;\n"
"// NOTE: Add your custom variables here\n"
"void main()\n"
"{\n"
"   // NOTE: Calculate alpha using signed distance field (SDF)\n"
"   float distanceFromOutline = texture(texture0, fragTexCoord).a - 0.5;\n"
"   float distanceChangePerFragment = length(vec2(dFdx(distanceFromOutline), dFdy(distanceFromOutline)));\n"
"   float alpha = smoothstep(-distanceChangePerFragment, distanceChangePerFragment, distanceFromOutline);\n"
"   // Calculate final fragment color\n"
"   finalColor = vec4(fragColor.rgb, fragColor.a*alpha);\n"
"}\n"
;
