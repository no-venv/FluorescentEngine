// This is a helper class that makes it easier for any nodes that utilizes custom GLSL code
#pragma once

#include "BKE_global.hh"
#include "BKE_main.hh"
#include "BLI_path_utils.hh"
#include "BLI_string_ref.hh"
#include "GPU_shader.hh"
#include "NOD_node_declaration.hh"
#include "NOD_socket_declarations.hh"
#include <functional>
#include <iostream>
#include <list>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

enum GlslUniformType { BOOL, INT, FLOAT, VEC2, VEC3, SAMPLER2D, UNDEFINED };

enum ShaderType { COMPOSITOR, MATERIAL };

union GlslUniformValue {
  float value_float;
  int value_int;
  float value_vec2[2];
  float value_vec3[3];
  float value_vec4[4];
  bool value_bool;
};

struct GlslUniform {
  GlslUniformType type = GlslUniformType::UNDEFINED;
  bool contains_value;
  GlslUniformValue value;
  std::string name;
};

struct InMemoryShaderData {
  // Stores the compiled shader in memory
  GPUShader *shader = nullptr;
  std::vector<GlslUniform> uniforms = {};
};

using ShaderMap = std::unordered_map<std::string, InMemoryShaderData>;
using ShaderCallbackMap =
    std::unordered_map<std::string, std::unordered_map<int32_t, std::function<void()>>>;

using namespace blender;
class NodeGlslHelper {

  ShaderMap compositor_shader_data_map;
  ShaderMap material_shader_data_map;

  ShaderCallbackMap compositor_shader_cb_map;
  ShaderCallbackMap material_shader_cb_map;

 private:
  ShaderMap *shader_data_from_type(ShaderType shader_type);
  ShaderCallbackMap *shader_cb_from_type(ShaderType shader_type);


 public:
  NodeGlslHelper();

  void setup_node_declaration(const char *shader_name,
                              nodes::NodeDeclarationBuilder &b,
                              ShaderType shader_type);

  void add_callback(const char *shader_name,
                    int32_t node_id,
                    ShaderType shader_type,
                    std::function<void()> callback);
  void remove_callback(const char *shader_name, int32_t node_id, ShaderType shader_type);
  void fire_callback(const char *shader_name, ShaderType shader_type);

  std::vector<std::string> get_loaded_shaders_names(ShaderType shader_type);
  InMemoryShaderData &get_shader(const char *shader_name, ShaderType shader_type);

  bool set_shader(const char *shader_name, const char *code, ShaderType shader_type);
};

extern NodeGlslHelper global_glsl_helper;
