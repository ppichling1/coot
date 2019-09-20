
#shader vertex

#version 330 core

layout(location = 0) in vec3 model_rotation_matrix_0;
layout(location = 1) in vec3 model_rotation_matrix_1;
layout(location = 2) in vec3 model_rotation_matrix_2;
layout(location = 3) in vec3 model_translation;
layout(location = 4) in vec3 position; // origin-based cylinder
layout(location = 5) in vec3 normal;   // ditto
layout(location = 6) in vec4 colour;

uniform mat4 mvp;
uniform mat4 view_rotation; // the quaternion attached to what the mouse has done
uniform vec4 background_colour;
uniform vec4 eye_position;

out vec3 frag_pos;
out vec3 Normal;
out vec4 tri_color;
out vec4 bg_colour;
out vec4 eye_position_transfer; // needed?

void main() {

   mat3 model_rotation_matrix = mat3(model_rotation_matrix_0, model_rotation_matrix_1, model_rotation_matrix_2);

   vec3 p2 = position * model_rotation_matrix;
   vec4 p3 = vec4(p2 + model_translation, 1.0);
   gl_Position = mvp * p3;

   vec4 n1 = vec4(normal * model_rotation_matrix, 1.0);
   vec4 n2 = view_rotation * n1;

   Normal = normalize(n2.xyz);
   frag_pos =  p3.xyz;
   tri_color = colour;
   bg_colour = background_colour;
   eye_position_transfer = eye_position;
}

#shader fragment

#version 330 core

in vec3 frag_pos;
in vec3 Normal;
in vec4 tri_color;
in vec4 bg_colour;
in vec4 eye_position_transfer;

layout(location = 0) out vec4 out_col;

void main() {

  vec4 light_colour = vec4(0.4, 0.4, 0.4, 1.0);
  float specular_strength = 0.8;
  vec3 lightdir_1 = normalize(vec3(-2, -2, 4)); // positive z means light from my side of the screen
  vec3 lightdir_2 = normalize(vec3( 2,  2, 4));
  float dp_l1 = dot(Normal, -lightdir_1);
  float dp_l2 = dot(Normal, -lightdir_2);

  float f_1 = 1.0 - gl_FragCoord.z; // because glm::ortho() near and far are reversed?
  // f_1 = gl_FragCoord.z;
  float f_2 = 1.0 - abs(f_1 - 0.7)/0.7;
  // f_2 = f_1; // just testing
  vec4 col_1 = vec4(vec3(f_2), 1.0) * tri_color;
  vec4 col_2_1 = col_1 * dp_l1;
  vec4 col_2_2 = col_1 * dp_l2;
  vec4 col_2 = col_2_1; // + col_2_2;

  float flat_frac = 0.1;
  // vec4 c_1 = col_2 * (1.0 - flat_frac) + col_1 * flat_frac;
  vec4 c_1 = mix(col_2, col_1, flat_frac);
  vec4 c_2 = mix(bg_colour, c_1, f_1);

  // is this right?
  vec3 eye_pos =  vec3(0.0, 0.0, 5.0);
  eye_pos = -eye_position_transfer.xyz;

  vec3 view_dir = eye_pos - frag_pos; // view_dir.z positive is a good idea.
  view_dir = normalize(view_dir);

  vec3 norm_2 = Normal;
  norm_2 = normalize(norm_2);
  vec3 reflect_dir = reflect(-lightdir_1, norm_2);

  float dp_view_reflect = dot(view_dir, reflect_dir);
  dp_view_reflect = max(dp_view_reflect, 0.0);

  float spec = pow(dp_view_reflect, 3.2);
  vec4 specular = specular_strength * spec * light_colour;
  vec4 c_3 = c_1 + specular;
  // c_3 = specular;
  vec4 c_4 = mix(bg_colour, c_3, f_1);


  out_col = c_4;

}