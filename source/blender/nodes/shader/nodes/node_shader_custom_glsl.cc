/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "BKE_global.hh"
#include "BKE_main.hh"
#include "BKE_node_runtime.hh"

#include "DNA_material_types.h"
#include "GPU_shader.hh"
#include "NOD_glsl_base.hh"
#include "NOD_glsl_helper.hh"
#include "NOD_socket.hh"

#include "RNA_access.hh"

#include "draw_manager_c.hh"
#include "eevee_private.hh"
#include "node_shader_util.hh"
#include "node_util.hh"

const ShaderType MATERIAL_SH_TYPE = ShaderType::MATERIAL;

namespace blender::nodes::node_shader_custom_glsl_cc {

static void node_init(bNodeTree *ntree, bNode *node)
{
  NodeShaderCustomGLSL *tex = MEM_cnew<NodeShaderCustomGLSL>(__func__);
  node->storage = tex;
}

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (node == nullptr) {
    return;
  }

  int32_t node_id = node->identifier;
  NodeShaderCustomGLSL *storage = (NodeShaderCustomGLSL *)node->storage;
  glsl_node_declare(b, &storage->base, MATERIAL_SH_TYPE, [node_id] {
    LISTBASE_FOREACH (Material *, mat, &G.main->materials) {
      if (mat->nodetree) {
        return mat->nodetree->node_by_id(node_id);
      }
    }
    return static_cast<bNode *>(nullptr);
  });
}

static int gpu_shader_exec_gl(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData * /*execdata*/,
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  auto storage = (NodeShaderCustomGLSL *)node->storage;
  auto base = &storage->base;
  auto shader = global_glsl_helper.get_shader(base->shader_name, MATERIAL_SH_TYPE);
  if (shader == nullptr) {
    return false;
  }
  // replace any blank Sampler2D inputs with a placeholder gpu texture
  auto iter = 0;
  for (auto i : shader->uniforms) {
    if (i.type == GlslUniformType::SAMPLER2D) {
      if (in[iter].link == nullptr) {
        in[iter].link = GPU_texture(mat, global_glsl_helper.placeholder_sampler2d_tex());
      }
    }
    iter++;
  }
  return GPU_stack_link(mat, node, shader->material_entry_point.c_str(), in, out);
}

}  // namespace blender::nodes::node_shader_custom_glsl_cc

void register_node_type_sh_custom_glsl()
{
  namespace file_ns = blender::nodes::node_shader_custom_glsl_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeCustomGlsl", SH_NODE_CUSTOM_GLSL);
  ntype.ui_name = "Custom GLSL";
  ntype.ui_description = "";
  ntype.enum_name_legacy = "SHADERNODECUSTOMGLSL";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_exec_gl;
  ntype.draw_buttons = glsl_node_draw_buttons;
  ntype.initfunc = file_ns::node_init;
  blender::bke::node_type_storage(
      &ntype, "NodeShaderCustomGLSL", node_free_standard_storage, node_copy_standard_storage);
  blender::bke::node_register_type(&ntype);
}

/*
// This needs to be revisited in the future:vvvv
// read uniforms to expose internal EEVEE buffer

// EEVEE_Data *vedata;
// EEVEE_StorageList *stl;
// EEVEE_EffectsInfo *effects;
// GPUNodeLink *gpu_image;
// std::deque<GPUNodeLink *> gpu_texture_node_link;
// DRW_ENABLED_ENGINE_ITER (DST.view_data_active, engine, data) {
//   // not sure if i can get this without looping... so,
//   if (engine == &draw_engine_eevee_type) {
//     vedata = (EEVEE_Data *)data;
//     stl = vedata->stl;
//     effects = stl->effects;
//     break;
//   }
// }

// for (auto uniform :
//      global_glsl_helper.get_shader(storage->shader_name, ShaderType::MATERIAL).uniforms)
// {

//   if (true) {
//     continue;
//   }

//   if (uniform.name == "SCREEN_TEXTURE") {
//     std::cout << "adding screen texture\n";
//     gpu_texture_node_link.push_back(GPU_texture(mat,
//     &DRW_viewport_texture_list_get()->color));
//   }
//   if (uniform.name == "DEPTH_TEXTURE") {
//     gpu_texture_node_link.push_back(GPU_texture(mat,
//     &DRW_viewport_texture_list_get()->depth));
//     // DRWViewData *view_data = DST.view_data_active;
//     // &view_data->dtxl;
//     // theres no buffer....
//   }
//   if (uniform.name == "NORMAL_TEXTURE") {
//     // effects->ssr_normal_input;
//   }
// }
// // This needs to be revisited in the future:^^^^^

*/
