

#pragma once

#include "NOD_glsl_helper.hh"
#include "NOD_node_declaration.hh"
#include "NOD_socket_declarations.hh"

// using namespace blender;

void glsl_node_declare(nodes::NodeDeclarationBuilder &b,
                       NodeGlslBase *storage,
                       ShaderType shader_type,
                       std::function<bNode *()> get_node);

void glsl_node_draw_buttons(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr);
