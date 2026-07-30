
#include "fluorescent_py_compositor.hh"
#include "NOD_glsl_helper.hh"
#include <Python.h>

// PyDoc_STRVAR(py_register_shader_doc,
//              ".. function:: register_shader(id,code)\n"
//              "\n"
//              "    Registers a custom compositor shader");

static PyObject *py_register_compute_shader(PyObject * /*self*/, PyObject *args)
{
  const char *id;
  const char *code;

  if (!PyArg_ParseTuple(args, "ss", &id, &code)) {
    return NULL;
  }

  global_glsl_helper.set_shader(id, code, ShaderType::COMPOSITOR);
  // update nodes
  global_glsl_helper.fire_callback(id, ShaderType::COMPOSITOR);
  Py_RETURN_NONE;
}

static PyObject *py_get_loaded_compute_shaders(PyObject * /*self*/, PyObject *args)
{
  auto loaded = global_glsl_helper.get_loaded_shaders_names(ShaderType::COMPOSITOR);
  auto size = loaded.size();
  PyObject *list = PyList_New(size);
  if (!list) {
    return NULL;
  }

  for (Py_ssize_t i = 0; i < size; i++) {
    PyObject *py_str = PyUnicode_FromString(loaded[i].c_str());
    if (!py_str) {
      Py_DECREF(list);
      return NULL;
    }
    PyList_SetItem(list, i, py_str);
  }
  return list;
}

static PyMethodDef fluorescent_compositor_methods[]{
    {"register_compute_shader", (PyCFunction)py_register_compute_shader, METH_VARARGS, nullptr},
    {"get_loaded_compute_shaders",
     (PyCFunction)py_get_loaded_compute_shaders,
     METH_NOARGS,
     nullptr},
    {nullptr, nullptr, 0, nullptr}};

PyDoc_STRVAR(
    /* Wrap. */
    fluorescent_compositor_doc,
    "Module for interfacing with fluorescent's compositor features");

static PyModuleDef fluorescent_compositor_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "fluorescent.compositor",
    /*m_doc*/ fluorescent_compositor_doc,
    /*m_size*/ 0,
    /*m_methods*/ fluorescent_compositor_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *bpy_fluorescent_compositor_init()
{
  PyObject *submodule;

  submodule = PyModule_Create(&fluorescent_compositor_def);

  return submodule;
}

/** \} */
