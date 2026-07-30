/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <Python.h>

#include "fluorescent_py_api.hh" /* Own include. */
#include "fluorescent_py_compositor.hh"
/* -------------------------------------------------------------------- */

PyDoc_STRVAR(
    /* Wrap. */
    fluorescent_doc,
    "Module for interfacing with fluorescent's features");

static PyModuleDef fluorescent_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "fluorescent",
    /*m_doc*/ fluorescent_doc,
    /*m_size*/ 0,
    /*m_methods*/ nullptr,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPyInit_fluorescent()
{
  PyObject *sys_modules = PyImport_GetModuleDict();
  PyObject *submodule;
  PyObject *mod;

  mod = PyModule_Create(&fluorescent_module_def);

  PyModule_AddObject(mod, "compositor", (submodule = bpy_fluorescent_compositor_init()));
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(submodule), submodule);
  return mod;
}

/** \} */
