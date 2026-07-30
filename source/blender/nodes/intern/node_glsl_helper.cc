// This is a helper class that makes it easier for any nodes that utilizes custom GLSL code
#include "NOD_glsl_helper.hh"

static const std::unordered_map<std::string, GlslUniformType> to_uniform_lookup = {
    {"float", GlslUniformType::FLOAT},
    {"int", GlslUniformType::INT},
    {"bool", GlslUniformType::BOOL},
    {"vec2", GlslUniformType::VEC2},
    {"vec3", GlslUniformType::VEC3},
    {"sampler2D", GlslUniformType::SAMPLER2D}};

GlslUniformType to_uniform_enum(const std::string &str)
{
  auto it = to_uniform_lookup.find(str);
  if (it != to_uniform_lookup.end()) {
    return it->second;
  }
  return GlslUniformType::UNDEFINED;
}

std::string remove_comments(const std::string &source)
{
  const std::regex rm_comment_regex(R"(/\*[\s\S]*?\*/|//.*)");
  return std::regex_replace(source, rm_comment_regex, "");
}

std::vector<GlslUniform> parse_uniforms(const std::string &source)
{
  std::vector<GlslUniform> uniforms;
  std::string source_no_comments = remove_comments(source);

  // regex for single-line uniforms
  const std::regex uniform_regex(
      R"(\buniform\s+([a-zA-Z0-9_]+)\s+([a-zA-Z0-9_]+)(?:\s*=\s*([^;]+))?\s*;)");
  // regex for extracting values in the uniform
  const std::regex uniform_value_regex(R"([^(),\s]+(?=[^()]*\)))");

  auto words_begin = std::sregex_iterator(
      source_no_comments.begin(), source_no_comments.end(), uniform_regex);
  auto words_end = std::sregex_iterator();

  for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
    std::smatch match = *i;
    GlslUniform uniform;
    uniform.type = to_uniform_enum(match[1].str());
    uniform.name = match[2].str();

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

NodeGlslHelper::NodeGlslHelper()
    : compositor_shader_data_map{},
      material_shader_data_map{},
      compositor_shader_cb_map{},
      material_shader_cb_map{}
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
  // ehhh, i'd wanted to return nullptr but this is good enough
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

void NodeGlslHelper::setup_node_declaration(const char *shader_name,
                                            nodes::NodeDeclarationBuilder &b,
                                            ShaderType shader_type)
{
  ShaderMap *shader_map = shader_data_from_type(shader_type);
  InMemoryShaderData *memshader = &(*shader_map)[shader_name];

  for (const auto &uniform : memshader->uniforms) {
    switch (uniform.type) {
      case GlslUniformType::BOOL: {
        b.add_input<nodes::decl::Bool>(uniform.name)
            .default_value(uniform.contains_value ? uniform.value.value_bool : false);
        break;
      }
      case GlslUniformType::FLOAT: {
        b.add_input<nodes::decl::Float>(uniform.name)
            .default_value(uniform.contains_value ? uniform.value.value_float : 0.0);
        break;
      }
      case GlslUniformType::INT: {
        b.add_input<nodes::decl::Int>(uniform.name)
            .default_value(uniform.contains_value ? uniform.value.value_int : 0);
        break;
      }
      case GlslUniformType::VEC2: {
        b.add_input<nodes::decl::Vector>(uniform.name)
            .default_value(
                uniform.contains_value ?
                    float3{uniform.value.value_vec2[0], uniform.value.value_vec2[1], 0.0} :
                    float3{0.0, 0.0, 0.0});
        break;
      }
      case GlslUniformType::VEC3: {
        b.add_input<nodes::decl::Vector>(uniform.name)
            .default_value(uniform.contains_value ? float3{uniform.value.value_vec3[0],
                                                           uniform.value.value_vec3[1],
                                                           uniform.value.value_vec3[2]} :
                                                    float3{0.0, 0.0, 0.0});
        break;
      }
      case GlslUniformType::SAMPLER2D: {
        b.add_input<nodes::decl::Color>(uniform.name);
        break;
      }
    }
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

InMemoryShaderData &NodeGlslHelper::get_shader(const char *shader_name, ShaderType shader_type)
{
  ShaderMap *shader_map = shader_data_from_type(shader_type);
  return (*shader_map)[shader_name];
}

bool NodeGlslHelper::set_shader(const char *shader_name, const char *code, ShaderType shader_type)
{
  // free gpu shader, if any.
  InMemoryShaderData &memshader = get_shader(shader_name, shader_type);
  GPU_SHADER_FREE_SAFE(memshader.shader);

  std::string buffer_str = std::string(code);

  const std::regex layout_header_regex(
      R"(^[^\/\n]*layout\s*\(\s*local_size_x\s*=\s*(\d+)\s*,\s*local_size_y\s*=\s*(\d+)\s*,\s*local_size_z\s*=\s*(\d+)\s*\)\s*in\s*;)",
      std::regex_constants::ECMAScript | std::regex_constants::multiline);

  auto shader_header = "layout(binding=0,rgba16f) uniform image2D output_img;\n";
  // determine if we're using a custom layout.
  auto header =
      "#define UV (vec2(gl_GlobalInvocationID.xy) + vec2(0.5)) / vec2(imageSize(output_img));\n"
      "#define OUTPUT(colour) imageStore(output_img, ivec2(gl_GlobalInvocationID.xy), colour);\n";

  if (!std::regex_search(buffer_str, layout_header_regex)) {
    // just append our own
    shader_header =
        "layout(binding=0,rgba16f) uniform image2D output_img;\n"
        "layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;\n";
  }

  GPUShader *shader = GPU_shader_create_compute(buffer_str.c_str(), shader_header, header, "");
  if (shader == nullptr) {
    return false;
  }
  memshader.shader = shader;
  memshader.uniforms = parse_uniforms(buffer_str);
  return true;
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

void NodeGlslHelper::fire_callback(const char *shader_name, ShaderType shader_type)
{
  ShaderCallbackMap *cb_map = shader_cb_from_type(shader_type);
  for (auto value : (*cb_map)[shader_name]) {
    value.second();
  }
}

NodeGlslHelper global_glsl_helper;
