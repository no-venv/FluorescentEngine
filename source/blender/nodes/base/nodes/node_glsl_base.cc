#include "BKE_node_runtime.hh"
#include "DNA_material_types.h"
#include "NOD_socket.hh"

#include "node_util.hh"

#include "BLI_string.h"
#include "NOD_glsl_base.hh"
#include "UI_interface.hh"

#include <functional>
#include <unordered_map>

std::unordered_map<int32_t, std::string> previous_set_shader = {};

static void glsl_node_copy_uniforms_to_internal(NodeGlslBase *storage, ShaderType shader_type)
{
  auto shader = global_glsl_helper.get_shader(storage->shader_name, shader_type);
  int uniform_len = 0;

  for (auto i : shader->uniforms) {
    if (uniform_len == 32) {
      break;
    }
    auto internal_uniforms = &storage->uniforms[uniform_len];
    internal_uniforms->type = i.type;
    internal_uniforms->qualifier = i.qualifier;
    STRNCPY(internal_uniforms->name, i.name);
    uniform_len++;
  }

  storage->uniform_size = uniform_len;
}

void glsl_node_declare(nodes::NodeDeclarationBuilder &b,
                       NodeGlslBase *storage,
                       ShaderType shader_type,
                       std::function<bNode *()> get_node)
{
  const bNode *node = b.node_or_null();
  if (node == nullptr) {
    return;
  }
  int32_t node_id = node->identifier;
  InMemoryShaderData *shader = nullptr;

  if (storage->shader_name[0] == '\0') {
    storage->is_file_shader = true;
    return;
  }

  if (!storage->is_file_shader) {
    // this a shader created by a Python script.
    shader = global_glsl_helper.get_shader(storage->shader_name, shader_type);
  }
  else {
    // get the shader based on file path, 'shader_name' is actually the filepath
    // currently, filepaths must be absolute. supporting relative paths are a pain in the ass with
    // no resolution yet.
    shader = global_glsl_helper.set_shader_from_file(
        storage->shader_name, storage->shader_name, shader_type);
  }
  if (shader == nullptr) {
    // default to copied uniforms from the last time the shader was working to avoid destroying
    // node links
    std::vector<GlslUniform> uniforms = {};
    for (int i; i < storage->uniform_size; i++) {
      auto stored_uniform = storage->uniforms[i];
      auto stored_uniform_type = static_cast<GlslUniformType>(stored_uniform.type);
      auto stored_uniform_qualiifer = static_cast<GlslQualifierType>(stored_uniform.qualifier);
      auto stored_uniform_name = *stored_uniform.name;
      uniforms.push_back(
          GlslUniform{stored_uniform_type, stored_uniform_qualiifer, stored_uniform_name, false});
    }
    global_glsl_helper.setup_node_declaration_ex(b, uniforms);
    return;
  }

  global_glsl_helper.setup_node_declaration(storage->shader_name, b, shader_type);
  const char *shader_name = storage->shader_name;
  // remove old callbacks before adding a new one,
  auto old_sh_name = previous_set_shader[node_id];
  global_glsl_helper.remove_callback(old_sh_name.c_str(), node_id, shader_type);
  global_glsl_helper.add_callback(
      shader_name, node->identifier, shader_type, [storage, get_node, shader_type] {
        auto node = get_node();
        if (node == nullptr) {
          return;
        }
        glsl_node_copy_uniforms_to_internal(storage, shader_type);
        bNodeTypeHandle typeinfo = *node->typeinfo;
        build_node_declaration(typeinfo, *node->runtime->declaration, &node->owner_tree(), node);
        node_verify_sockets(&node->owner_tree(), node, true);
        return;
      });
  glsl_node_copy_uniforms_to_internal(storage, shader_type);
  previous_set_shader[node_id] = storage->shader_name;
}

void glsl_node_draw_buttons(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "shader_name", UI_ITEM_NONE, "", ICON_NONE);
}
