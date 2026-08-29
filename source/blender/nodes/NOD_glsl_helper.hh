// This is a helper singleton that makes it easier for any nodes that utilizes custom GLSL code
#pragma once

#include "GPU_shader.hh"
#include "NOD_socket_declarations.hh"

#include <filesystem>
#include <functional>
#include <list>
#include <tuple>
#include <unordered_map>
#include <vector>

enum GlslUniformType : int { BOOL, INT, FLOAT, VEC2, VEC3, VEC4, SAMPLER2D, UNDEFINED };

enum GlslQualifierType : int { IN, OUT };

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
  GlslQualifierType qualifier = GlslQualifierType::IN;
  char name[16];
  bool contains_value;
  GlslUniformValue value;
};

struct InMemoryShaderData {
  // Stores the compiled shader in memory
  GPUShader *shader = nullptr;
  std::string material_src;
  std::string material_filename;
  std::string material_filepath;
  std::string material_entry_point;
  std::string uid;
  std::vector<GlslUniform> uniforms = {};
  std::vector<std::string> void_functions = {};
};

using ShaderMap = std::unordered_map<std::string, InMemoryShaderData>;
using ShaderCallbackMap =
    std::unordered_map<std::string, std::unordered_map<int32_t, std::function<void()>>>;

using LoadedFolderShaders = std::tuple<std::vector<std::string>, std::vector<std::string>>;
using namespace blender;
class NodeGlslHelper {

  ShaderMap compositor_shader_data_map;
  ShaderMap material_shader_data_map;

  ShaderCallbackMap compositor_shader_cb_map;
  ShaderCallbackMap material_shader_cb_map;
  GPUTexture *blank_tex;

 private:
  ShaderMap *shader_data_from_type(ShaderType shader_type);
  ShaderCallbackMap *shader_cb_from_type(ShaderType shader_type);

 public:
  NodeGlslHelper();

  void setup_node_declaration_ex(nodes::NodeDeclarationBuilder &b,
                                 std::vector<GlslUniform> uniforms);

  void setup_node_declaration(const char *shader_name,
                              nodes::NodeDeclarationBuilder &b,
                              ShaderType shader_type);

  void add_callback(const char *shader_name,
                    int32_t node_id,
                    ShaderType shader_type,
                    std::function<void()> callback);

  void remove_callback(const char *shader_name, int32_t node_id, ShaderType shader_type);

  void fire_callback(const char *shader_name, int32_t node_id, ShaderType shader_type);
  void fire_callback(const char *shader_name, ShaderType shader_type);

  std::vector<std::string> get_loaded_shaders_names(ShaderType shader_type);

  void remove_shader(const char *shader_name, ShaderType shader_type);

  InMemoryShaderData *get_shader(const char *shader_name,
                                 ShaderType shader_type,
                                 bool auto_create = false);
  InMemoryShaderData *set_shader(const char *shader_name,
                                 const char *code,
                                 ShaderType shader_type);
  InMemoryShaderData *set_shader_from_file(const char *filepath,
                                           const char *shader_name,
                                           ShaderType shader_type);

  GPUTexture *placeholder_sampler2d_tex();
};

extern NodeGlslHelper global_glsl_helper;
