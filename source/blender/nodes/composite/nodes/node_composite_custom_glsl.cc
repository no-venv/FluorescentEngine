/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"
#include "GPU_shader.hh"
#include "NOD_glsl_helper.hh"
#include "UI_interface.hh"
#include "UI_resources.hh"
#include "WM_api.hh"
#include "node_composite_util.hh"
/* **************** CUSTOM GLSL ******************** */

NODE_STORAGE_FUNCS(NodeCompositorCustomGLSL)

namespace blender::nodes::node_composite_customglsl_cc {

static void cmp_node_customglsl_declare(NodeDeclarationBuilder &b)
{
  // Delares the inputs/outputs of the node
  const bNode *node = b.node_or_null();
  if (node != nullptr) {
    NodeCompositorCustomGLSL storage = node_storage(*node);
    int32_t node_id = node->identifier;
    global_glsl_helper.setup_node_declaration(storage.shader_name, b, ShaderType::COMPOSITOR);
    global_glsl_helper.add_callback(
        storage.shader_name, node->identifier, ShaderType::COMPOSITOR, [node_id] {
          LISTBASE_FOREACH (Scene *, scene, &G.main->scenes) {
            if (scene->nodetree) {
              bNode *node = scene->nodetree->node_by_id(node_id);
              if (node == nullptr) {
                return;
              }

              bNodeTypeHandle typeinfo = *node->typeinfo;
              build_node_declaration(typeinfo, *node->runtime->declaration, scene->nodetree, node);
              node_verify_sockets(scene->nodetree, node, true);
              return;
            }
          }
        });
  }
  b.add_output<decl::Color>("Image");
}

static void node_composite_init_glsl(bNodeTree * /*ntree*/, bNode *node)
{
  NodeCompositorCustomGLSL *nccg = MEM_cnew<NodeCompositorCustomGLSL>(__func__);
  node->storage = nccg;
}

static void node_free(bNode *node)
{
  // this doesn't fire?
  NodeCompositorCustomGLSL storage = node_storage(*node);
  global_glsl_helper.remove_callback(
      storage.shader_name, node->identifier, ShaderType::COMPOSITOR);
}

using namespace blender::compositor;

class CompositorCustomGLSLOperation : public NodeOperation {

 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (this->context().use_gpu()) {
      this->execute_gpu();
    }
  }

  void execute_gpu()
  {
    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);

    bNode node = bnode();
    NodeCompositorCustomGLSL storage = node_storage(node);
    InMemoryShaderData &helper = global_glsl_helper.get_shader(storage.shader_name,
                                                               ShaderType::COMPOSITOR);
    GPUShader *shader = helper.shader;

    if (shader == nullptr) {
      return;
    }

    GPU_shader_bind(shader);
    // set shader arguments by extracted uniform
    std::vector<Result> binded_textures;
    for (const auto &uniform : helper.uniforms) {
      auto name = uniform.name.c_str();
      switch (uniform.type) {
        case GlslUniformType::BOOL: {
          GPU_shader_uniform_1b(shader, name, get_input(name));
          break;
        }
        case GlslUniformType::FLOAT: {
          GPU_shader_uniform_1f(shader, name, get_input(name).get_single_value<float>());
          break;
        }
        case GlslUniformType::INT: {
          GPU_shader_uniform_1i(shader, name, get_input(name).get_single_value<int>());
          break;
        }
        case GlslUniformType::SAMPLER2D: {
          const Result &input_image = get_input(name);
          input_image.bind_as_texture(shader, name);
          binded_textures.push_back(input_image);
          break;
        }
        case GlslUniformType::VEC2: {
          GPU_shader_uniform_2fv(shader, name, get_input(name).get_single_value<float2>());
          break;
        }
        case GlslUniformType::VEC3: {
          GPU_shader_uniform_3fv(shader, name, get_input(name).get_single_value<float3>());
          break;
        }
      }
    }
    output_image.bind_as_image(shader, "output_img");
    compute_dispatch_threads_at_least(shader, domain.size);
    GPU_shader_unbind();
    output_image.unbind_as_image();
    for (const auto &texture : binded_textures) {
      texture.unbind_as_texture();
    }
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new CompositorCustomGLSLOperation(context, node);
}

}  // namespace blender::nodes::node_composite_customglsl_cc

void register_node_type_cmp_custom_glsl()
{
  namespace file_ns = blender::nodes::node_composite_customglsl_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeCustomGlsl", CMP_NODE_CUSTOM_GLSL);
  ntype.ui_name = "Custom GLSL Shader";
  ntype.ui_description = "Custom shader that can be used in the compositor pass";
  ntype.enum_name_legacy = "GLSLSHADERCUSTOM";  // not really important-
  ntype.nclass = NODE_CLASS_SHADER;             // where the node belongs to,
  ntype.declare = file_ns::cmp_node_customglsl_declare;
  ntype.initfunc = file_ns::node_composite_init_glsl;
  ntype.freefunc = file_ns::node_free;
  blender::bke::node_type_storage(
      &ntype, "NodeCompositorCustomGLSL", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  blender::bke::node_register_type(&ntype);
}

// static void node_composite_buts_customglsl(uiLayout *layout, bContext * /*C*/, PointerRNA
// *ptr)
// {
//   // draws the buttons on the ui
//   uiLayout *row;
//   // for (now), we'd only support external files
//   // uiItemR expects a pointer to the RNA, which would index the property
//   row = uiLayoutRow(layout, true);
//   uiItemR(row, ptr, "filepath", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
// }

// for some reason, the rebuilding of the node will actually reset the values
// previously stored, at least when the node starts up the first time

// NodeDeclarationBuilder node_decl_builder{
//     typeinfo, *node->runtime->declaration, scene->nodetree, node};
// typeinfo.declare(node_decl_builder);
// node_decl_builder.finalize();
