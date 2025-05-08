//
// vertex.glsl
//
//  Copyright © 2025 Robert Guequierre
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#version 450
#pragma shader_stage(vertex)

// * Clockwise triangle strip quad
//
//   1   3
//   | \ |
//   0   2
//
vec2 positions[4] = vec2[] (

    vec2(-0.5,  0.5),
    vec2(-0.5, -0.5),
    vec2( 0.5,  0.5),
    vec2( 0.5, -0.5)
);

void main() 
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}

