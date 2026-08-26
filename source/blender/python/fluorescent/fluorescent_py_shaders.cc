
#include "fluorescent_py_shaders.hh"
#include "NOD_glsl_helper.hh"
#include <Python.h>

// PyDoc_STRVAR(py_register_shader_doc,
//              ".. function:: register_shader(id,code)\n"
//              "\n"
//              "    Registers a custom compositor shader");

static void py_register_shader_ex(std::string shader_name,
                                  const char *code,
                                  ShaderType shader_type)
{
  auto appended_id = "PY_" + shader_name;
  global_glsl_helper.set_shader(appended_id.c_str(), code, shader_type);
  global_glsl_helper.fire_callback(appended_id.c_str(), shader_type);
}

static PyObject *py_register_compute_shader(PyObject * /*self*/, PyObject *args)
{
  const char *id;
  const char *code;

  if (!PyArg_ParseTuple(args, "ss", &id, &code)) {
    return NULL;
  }
  py_register_shader_ex(id, code, ShaderType::COMPOSITOR);
  Py_RETURN_NONE;
}

static PyObject *py_register_material_shader(PyObject * /*self*/, PyObject *args)
{
  const char *id;
  const char *code;

  if (!PyArg_ParseTuple(args, "ss", &id, &code)) {
    return NULL;
  }
  py_register_shader_ex(id, code, ShaderType::MATERIAL);
  Py_RETURN_NONE;
}

static PyObject *py_get_loaded_shader_ex(ShaderType shader_type)
{
  auto loaded = global_glsl_helper.get_loaded_shaders_names(shader_type);
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

static PyObject *py_get_loaded_compute_shaders(PyObject * /*self*/, PyObject *args)
{
  return py_get_loaded_shader_ex(ShaderType::COMPOSITOR);
}

static PyObject *py_get_loaded_material_shaders(PyObject * /*self*/, PyObject *args)
{
  return py_get_loaded_shader_ex(ShaderType::MATERIAL);
}

static PyMethodDef fluorescent_compositor_methods[]{
    {"register_compute_shader", (PyCFunction)py_register_compute_shader, METH_VARARGS, nullptr},
    {"register_material_shader", (PyCFunction)py_register_material_shader, METH_VARARGS, nullptr},
    {"get_loaded_compute_shaders",
     (PyCFunction)py_get_loaded_compute_shaders,
     METH_NOARGS,
     nullptr},
    {"get_loaded_material_shaders",
     (PyCFunction)py_get_loaded_material_shaders,
     METH_NOARGS,
     nullptr},
    {nullptr, nullptr, 0, nullptr}};

PyDoc_STRVAR(
    /* Wrap. */
    fluorescent_compositor_doc,
    "Module for interfacing with fluorescent's compositor features");

static PyModuleDef fluorescent_compositor_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "fluorescent.shaders",
    /*m_doc*/ fluorescent_compositor_doc,
    /*m_size*/ 0,
    /*m_methods*/ fluorescent_compositor_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *bpy_fluorescent_shaders_init()
{
  PyObject *submodule;

  submodule = PyModule_Create(&fluorescent_compositor_def);

  return submodule;
}

/** \} */
