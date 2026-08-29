// This is a helper class that makes it easier for any nodes that utilizes custom GLSL code
#include "BKE_global.hh"
#include "BKE_main.hh"
#include "BKE_text.h"

#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_uuid.h"

#include "NOD_glsl_helper.hh"

#include "DNA_text_types.h"
#include "DNA_uuid_types.h"

#include "gpu_material_library.hh"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

namespace {

const std::unordered_map<std::string, GlslUniformType> to_uniform_lookup = {
    {"float", GlslUniformType::FLOAT},
    {"int", GlslUniformType::INT},
    {"bool", GlslUniformType::BOOL},
    {"vec2", GlslUniformType::VEC2},
    {"vec3", GlslUniformType::VEC3},
    {"vec4", GlslUniformType::VEC4},
    {"sampler2D", GlslUniformType::SAMPLER2D}};

const std::unordered_map<std::string, GlslQualifierType> to_qualifier_lookup = {
    {"in", GlslQualifierType::IN}, {"out", GlslQualifierType::OUT}};

GlslUniformType to_uniform_enum(const std::string &str)
{
  auto it = to_uniform_lookup.find(str);
  if (it != to_uniform_lookup.end()) {
    return it->second;
  }
  return GlslUniformType::UNDEFINED;
}

GlslQualifierType to_qualifier_enum(const std::string &str)
{
  auto it = to_qualifier_lookup.find(str);
  if (it != to_qualifier_lookup.end()) {
    return it->second;
  }
  return GlslQualifierType::IN;
}

std::string remove_comments(const std::string &source)
{
  const std::regex rm_comment_regex(R"(/\*[\s\S]*?\*/|//.*)");
  return std::regex_replace(source, rm_comment_regex, "");
}

std::vector<std::string> parse_void_functions(const std::string &source)
{
  const std::regex regex_void_func(R"(\bvoid\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\()");
  std::vector<std::string> functions;

  auto clean_src = remove_comments(source);
  auto iter = std::sregex_iterator(clean_src.begin(), clean_src.end(), regex_void_func);
  auto iter_end = std::sregex_iterator();

  for (std::sregex_iterator j = iter; j != iter_end; ++j) {
    std::smatch function_name = *j;
    functions.push_back(function_name[1].str());
  }

  return functions;
}

std::vector<GlslUniform> parse_uniform_material(const std::string &source,
                                                const std::string shader_name)
{

  std::regex regex_func(R"((\w+)[ \t\r\n]+)" + shader_name + R"([ \t\r\n]*\(([^)]*)\))");
  std::regex regex_param(
      R"((?:(in|out|inout)[ \t\r\n]+)?([A-Za-z_]\w*(?:[ \t\r\n]*\[[ \t\r\n]*\w*[ \t\r\n]*\])?)[ \t\r\n]+([A-Za-z_]\w*)(?:[ \t\r\n]*\[[ \t\r\n]*\w*[ \t\r\n]*\])?)");
  std::smatch func_match;
  std::vector<GlslUniform> uniforms;

  auto clean_src = remove_comments(source);

  if (std::regex_search(clean_src, func_match, regex_func)) {
    auto raw_params = func_match[2].str();
    auto param_start = std::sregex_iterator(raw_params.begin(), raw_params.end(), regex_param);
    auto param_end = std::sregex_iterator();

    for (std::sregex_iterator j = param_start; j != param_end; ++j) {
      std::smatch match = *j;
      auto qualifier = match[1].matched ? match[1].str() : "none";
      auto type = match[2].str();
      auto name = match[3].str();
      GlslUniform uniform;
      uniform.type = to_uniform_enum(type);
      uniform.qualifier = to_qualifier_enum(qualifier);
      STRNCPY(uniform.name, name.c_str());
      uniforms.push_back(uniform);
    }
  }
  return uniforms;
}

std::vector<GlslUniform> parse_uniforms(const std::string &source)
{
  std::vector<GlslUniform> uniforms;
  std::string clean_src = remove_comments(source);
  // regex for single-line uniforms
  const std::regex uniform_regex(
      R"(\buniform\s+([a-zA-Z0-9_]+)\s+([a-zA-Z0-9_]+)(?:\s*=\s*([^;]+))?\s*;)");
  // regex for extracting values in the uniform
  const std::regex uniform_value_regex(R"([^(),\s]+(?=[^()]*\)))");

  auto src_start = std::sregex_iterator(clean_src.begin(), clean_src.end(), uniform_regex);
  auto src_end = std::sregex_iterator();

  for (std::sregex_iterator i = src_start; i != src_end; ++i) {
    std::smatch match = *i;
    GlslUniform uniform;
    uniform.type = to_uniform_enum(match[1].str());
    auto uniform_name = match[2].str();
    STRNCPY(uniform.name, uniform_name.c_str());

    if (match[3].matched) {
      std::string val = match[3].str();
      std::vector<std::string> values(std::regex_token_iterator<std::string::iterator>(
                                          val.begin(), val.end(), uniform_value_regex),
                                      std::regex_token_iterator<std::string::iterator>());
      size_t value_size = values.size();

      switch (uniform.type) {
        case GlslUniformType::BOOL: {
          size_t found = val.find("true");
          uniform.value.value_bool = found != std::string::npos;
          uniform.contains_value = true;
          break;
        }
        case GlslUniformType::FLOAT: {
          if (value_size == 0) {
            uniform.value.value_float = std::stof(val);
          }
          else {
            uniform.value.value_float = std::stof(values[0]);
          }
          uniform.contains_value = true;
          break;
        }
        case GlslUniformType::INT: {
          if (value_size == 0) {
            uniform.value.value_int = std::stoi(val);
          }
          else {
            uniform.value.value_int = std::stoi(values[0]);
          }
          uniform.contains_value = true;
          break;
        }
        case GlslUniformType::VEC2: {
          float x = std::stof(values[0]);
          float y = x;
          if (value_size != 0) {
            y = std::stof(values[1]);
          }
          uniform.value.value_vec2[0] = x;
          uniform.value.value_vec2[1] = y;
          uniform.contains_value = true;
          break;
        }
        case GlslUniformType::VEC3: {
          float x = std::stof(values[0]);
          float y = x;
          float z = x;
          if (value_size != 0) {
            y = std::stof(values[1]);
            z = std::stof(values[2]);
          }
          uniform.value.value_vec3[0] = x;
          uniform.value.value_vec3[1] = y;
          uniform.value.value_vec3[2] = z;
          uniform.contains_value = true;
          break;
        }
      }
    }
    uniforms.push_back(uniform);
  }

  return uniforms;
}
}  // namespace

NodeGlslHelper::NodeGlslHelper()
    : compositor_shader_data_map{},
      material_shader_data_map{},
      compositor_shader_cb_map{},
      material_shader_cb_map{},
      blank_tex(nullptr)
{
}

ShaderMap *NodeGlslHelper::shader_data_from_type(ShaderType shader_type)
{
  switch (shader_type) {
    case ShaderType::COMPOSITOR: {
      return &compositor_shader_data_map;
    }
    case ShaderType::MATERIAL: {
      return &material_shader_data_map;
    }
  }
  return &compositor_shader_data_map;
}

ShaderCallbackMap *NodeGlslHelper::shader_cb_from_type(ShaderType shader_type)
{
  switch (shader_type) {
    case ShaderType::COMPOSITOR: {
      return &compositor_shader_cb_map;
    }
    case ShaderType::MATERIAL: {
      return &compositor_shader_cb_map;
    }
  }
  return &compositor_shader_cb_map;
}

void NodeGlslHelper::setup_node_declaration_ex(nodes::NodeDeclarationBuilder &b,
                                               std::vector<GlslUniform> uniforms)
{
  for (const GlslUniform uniform : uniforms) {
    switch (uniform.type) {
      case GlslUniformType::BOOL: {
        auto &builder = uniform.qualifier == GlslQualifierType::IN ?
                            b.add_input<nodes::decl::Bool>(uniform.name) :
                            b.add_output<nodes::decl::Bool>(uniform.name);

        builder.default_value(uniform.contains_value ? uniform.value.value_bool : false);
        break;
      }
      case GlslUniformType::FLOAT: {
        auto &builder = uniform.qualifier == GlslQualifierType::IN ?
                            b.add_input<nodes::decl::Float>(uniform.name) :
                            b.add_output<nodes::decl::Float>(uniform.name);

        builder.default_value(uniform.contains_value ? uniform.value.value_float : 0.0);
        break;
      }
      case GlslUniformType::INT: {
        auto &builder = uniform.qualifier == GlslQualifierType::IN ?
                            b.add_input<nodes::decl::Int>(uniform.name) :
                            b.add_output<nodes::decl::Int>(uniform.name);

        builder.default_value(uniform.contains_value ? uniform.value.value_int : 0);
        break;
      }
      case GlslUniformType::VEC2: {
        auto &builder = uniform.qualifier == GlslQualifierType::IN ?
                            b.add_input<nodes::decl::Vector>(uniform.name) :
                            b.add_output<nodes::decl::Vector>(uniform.name);

        builder.default_value(
            uniform.contains_value ?
                float3{uniform.value.value_vec2[0], uniform.value.value_vec2[1], 0.0} :
                float3{0.0, 0.0, 0.0});
        break;
      }
      case GlslUniformType::VEC3: {
        auto &builder = uniform.qualifier == GlslQualifierType::IN ?
                            b.add_input<nodes::decl::Vector>(uniform.name) :
                            b.add_output<nodes::decl::Vector>(uniform.name);

        builder.default_value(uniform.contains_value ? float3{uniform.value.value_vec3[0],
                                                              uniform.value.value_vec3[1],
                                                              uniform.value.value_vec3[2]} :
                                                       float3{0.0, 0.0, 0.0});
        break;
      }
      case GlslUniformType::VEC4: {
        // Vec4's are interpreted as colours, for now.
        auto &builder = uniform.qualifier == GlslQualifierType::IN ?
                            b.add_input<nodes::decl::Color>(uniform.name) :
                            b.add_output<nodes::decl::Color>(uniform.name);

        // builder.default_value(uniform.contains_value ? float3{uniform.value.value_vec3[0],
        //                                                       uniform.value.value_vec3[1],
        //                                                       uniform.value.value_vec3[2]} :
        //                                                float3{0.0, 0.0, 0.0});
        break;
      }
      case GlslUniformType::SAMPLER2D: {
        b.add_input<nodes::decl::Color>(uniform.name);
        break;
      }
    }
  }
}

void NodeGlslHelper::setup_node_declaration(const char *shader_name,
                                            nodes::NodeDeclarationBuilder &b,
                                            ShaderType shader_type)
{
  auto memshader = get_shader(shader_name, shader_type);
  if (memshader != nullptr) {
    setup_node_declaration_ex(b, memshader->uniforms);
  }
}

std::vector<std::string> NodeGlslHelper::get_loaded_shaders_names(ShaderType shader_type)
{
  std::vector<std::string> loaded_shaders;
  ShaderMap *shader_map = shader_data_from_type(shader_type);
  for (auto value : *shader_map) {
    loaded_shaders.push_back(value.first);
  };
  return loaded_shaders;
}

InMemoryShaderData *NodeGlslHelper::get_shader(const char *shader_name,
                                               ShaderType shader_type,
                                               bool auto_create)
{
  ShaderMap *shader_map = shader_data_from_type(shader_type);
  if (shader_map->find(shader_name) == shader_map->end() && !auto_create) {
    return nullptr;
  }
  return &(*shader_map)[shader_name];
}

void NodeGlslHelper::remove_shader(const char *shader_name, ShaderType shader_type)
{
  auto memshader = get_shader(shader_name, shader_type, false);
  if (memshader == nullptr) {
    return;
  }
  GPU_SHADER_FREE_SAFE(memshader->shader);
  shader_data_from_type(shader_type)->erase(shader_name);
  shader_cb_from_type(shader_type)->erase(shader_name);
}

InMemoryShaderData *NodeGlslHelper::set_shader(const char *shader_name,
                                               const char *code,
                                               ShaderType shader_type)
{
  std::string buffer_str = std::string(code);
  InMemoryShaderData *memshader = get_shader(shader_name, shader_type, true);
  GPU_SHADER_FREE_SAFE(memshader->shader);
  // generate a uid for this run
  auto uuid = BLI_uuid_generate_random();
  char uuid_str[37];
  BLI_uuid_format(uuid_str, uuid);
  memshader->uid = std::string(uuid_str);

  if (shader_type == ShaderType::COMPOSITOR) {
    const std::regex layout_header_regex(
        R"(^[^\/\n]*layout\s*\(\s*local_size_x\s*=\s*(\d+)\s*,\s*local_size_y\s*=\s*(\d+)\s*,\s*local_size_z\s*=\s*(\d+)\s*\)\s*in\s*;)",
        std::regex_constants::ECMAScript | std::regex_constants::multiline);

    auto layout_header = "layout(binding=0,rgba16f) uniform image2D output_img;\n";

    auto define_header =
        "#define UV (vec2(gl_GlobalInvocationID.xy) + vec2(0.5)) / vec2(imageSize(output_img));\n"
        "#define OUTPUT(colour) imageStore(output_img, ivec2(gl_GlobalInvocationID.xy), "
        "colour);\n";

    // determine if we're using a custom layout
    if (!std::regex_search(buffer_str, layout_header_regex)) {
      // just append our own
      layout_header =
          "layout(binding=0,rgba16f) uniform image2D output_img;\n"
          "layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;\n";
    }

    GPUShader *shader = GPU_shader_create_compute(
        buffer_str.c_str(), layout_header, define_header, "");

    if (shader == nullptr) {
      return nullptr;
    }

    memshader->shader = shader;
    memshader->uniforms = parse_uniforms(buffer_str);
  }
  else {
    // get the list of functions with a VOID return type, because those are going to be
    // registered and need to be tracked
    auto functions = parse_void_functions(buffer_str);
    std::string material_entry_point;

    for (auto i : functions) {
      // find the main entry point of the shader.
      // usually it's like, "__main__function_name"
      if (StringRef(i.c_str()).startswith("__main__")) {
        // thats our main
        material_entry_point = i;
        break;
      }
    }

    if (material_entry_point.empty()) {
      return nullptr;
    }

    remove_runtime_shader_source(memshader->material_filename.c_str(),
                                 memshader->material_entry_point.c_str());

    for (auto i : memshader->void_functions) {
      remove_runtime_shader_source(nullptr, i.c_str());
    }

    memshader->void_functions = functions;
    memshader->material_entry_point = material_entry_point;
    memshader->material_src = preprocess_source(code);
    memshader->material_filename = std::string("gpu_shader_material_") + material_entry_point +
                                   ".glsl";
    memshader->material_filepath = std::string("/internal/gpu_shader_material_") +
                                   material_entry_point + ".glsl";
    memshader->uniforms = parse_uniform_material(buffer_str, material_entry_point);

    add_runtime_shader_source(memshader->material_src.c_str(),
                              memshader->material_filename.c_str(),
                              memshader->material_filepath.c_str());
  }

  return memshader;
}

InMemoryShaderData *NodeGlslHelper::set_shader_from_file(const char *filepath,
                                                         const char *shader_name,
                                                         ShaderType shader_type)
{
  std::ifstream file(filepath);
  if (!file.is_open())
    return nullptr;

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string file_contents = buffer.str();
  return set_shader(shader_name, file_contents.c_str(), shader_type);
}

void NodeGlslHelper::add_callback(const char *shader_name,
                                  int32_t node_id,
                                  ShaderType shader_type,
                                  std::function<void()> callback)
{
  ShaderCallbackMap *cb_map = shader_cb_from_type(shader_type);
  (*cb_map)[shader_name][node_id] = callback;
}

void NodeGlslHelper::remove_callback(const char *shader_name,
                                     int32_t node_id,
                                     ShaderType shader_type)
{
  ShaderCallbackMap *cb_map = shader_cb_from_type(shader_type);
  (*cb_map)[shader_name].erase(node_id);
}

void NodeGlslHelper::fire_callback(const char *shader_name,
                                   int32_t node_id,
                                   ShaderType shader_type)
{
  ShaderCallbackMap *cb_map = shader_cb_from_type(shader_type);
  auto map = (*cb_map)[shader_name];
  auto cb = map.find(node_id);
  if (cb == map.end()) {
    return;
  }
  cb->second();
}

void NodeGlslHelper::fire_callback(const char *shader_name, ShaderType shader_type)
{
  ShaderCallbackMap *cb_map = shader_cb_from_type(shader_type);
  for (auto value : (*cb_map)[shader_name]) {
    value.second();
  }
}

// Returns a placeholder sampler2D
GPUTexture *NodeGlslHelper::placeholder_sampler2d_tex()
{
  if (blank_tex != nullptr) {
    return blank_tex;
  }
  blank_tex = GPU_texture_create_2d("blank_sampler_tex_image",
                                    1,
                                    1,
                                    1,
                                    GPU_RGBA8,
                                    GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT,
                                    nullptr);
  return blank_tex;
}
NodeGlslHelper global_glsl_helper;
