# SPDX-FileCopyrightText: 2021 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  include
  ..
  ../intern
  ../../editors/include
  ../../makesrna
  ../../../../intern/sky/include
  ../../gpu/intern
  ../../draw
  ../../draw/intern
  ../../draw/engines/gooengine
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  nodes/node_glsl_base.cc
  include/NOD_glsl_base.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::functions
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  bf_intern_sky
  bf_nodes
  PRIVATE bf::render
  PRIVATE bf::windowmanager
)

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the STL versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

blender_add_lib(bf_nodes_base "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

blender_set_target_unity_build(bf_nodes_base 10)

# RNA_prototypes.hh
add_dependencies(bf_nodes_base bf_rna)
