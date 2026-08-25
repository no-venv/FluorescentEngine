# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../../nodes
  ../../makesrna
  # ../../editors/include
)

set(INC_SYS
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  fluorescent_py_api.cc
  fluorescent_py_shaders.cc

  fluorescent_py_api.hh
  fluorescent_py_shaders.hh
)

set(LIB
  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::gpu
  # PRIVATE bf::imbuf
    PRIVATE bf::blentranslation

  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::nodes

)
# add_library( bf_nodes)

# if(WITH_OPENGL_BACKEND)
#   list(APPEND INC_SYS
#     ${Epoxy_INCLUDE_DIRS}
#   )
#   list(APPEND LIB
#     ${Epoxy_LIBRARIES}
#   )
# endif()

blender_add_lib(bf_python_fluorescent "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
