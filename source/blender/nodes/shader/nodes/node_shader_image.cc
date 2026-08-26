/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_image_types.h"
#include "UI_interface.hh"
#include "node_shader_util.hh"

#include "BKE_node_runtime.hh"
#include "DNA_material_types.h"
#include "GPU_shader.hh"
#include "NOD_socket.hh"
#include "draw_manager_c.hh"
#include "eevee_private.hh"
#include "gpu_material_library.hh"
#include "node_shader_util.hh"
#include "node_util.hh"

namespace blender::nodes::node_shader_image_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Image");
}

static void node_layout(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiTemplateID(layout, C, ptr, "image", "IMAGE_OT_new", "IMAGE_OT_open", nullptr);
}

static int node_exec(GPUMaterial *mat,
                     bNode *node,
                     bNodeExecData * /*execdata*/,
                     GPUNodeStack *in,
                     GPUNodeStack *out)
{
  GPUSamplerState sampler_state = GPUSamplerState::default_sampler();
  Image *ima = (Image *)node->id;
  out[0].link = GPU_image(mat, ima, nullptr, sampler_state);
  return true;
}

}  // namespace blender::nodes::node_shader_image_cc

void register_node_type_sh_image()
{
  namespace file_ns = blender::nodes::node_shader_image_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeInputImage", SH_NODE_IMAGE);

  ntype.ui_name = "Sampler2D Image";
  ntype.ui_description = "Input an image data-block";
  ntype.enum_name_legacy = "IMAGE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.declare = file_ns::node_declare;
  blender::bke::node_register_type(&ntype);
}
