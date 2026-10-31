/* Optional, zero-copy scans of CPython's 1/2/4-byte Unicode storage. */
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#if defined(_M_X64) || defined(__SSE2__)
#include <emmintrin.h>
#define TEXT_SSE2 1
#ifdef _MSC_VER
#include <intrin.h>
#endif
#endif

#ifdef TEXT_SSE2
/* The caller has already checked that the mask is nonzero. */
static unsigned first_set(unsigned mask)
{
#ifdef _MSC_VER
    unsigned long first;
    _BitScanForward(&first, mask);
    return first;
#else
    return __builtin_ctz(mask);
#endif
}
#endif

/* Stop at a tag opener; never load beyond the string. */
static Py_ssize_t tag_start(const void *data, int kind, Py_ssize_t pos, Py_ssize_t size)
{
#ifdef TEXT_SSE2
    const Py_ssize_t lanes = 16 / kind;
    const __m128i needle = kind == 1 ? _mm_set1_epi8('<') :
                          kind == 2 ? _mm_set1_epi16('<') : _mm_set1_epi32('<');
    while (size - pos >= lanes) {
        const __m128i value = _mm_loadu_si128((const __m128i *)((const char *)data + pos * kind));
        const __m128i cmp = kind == 1 ? _mm_cmpeq_epi8(value, needle) :
                            kind == 2 ? _mm_cmpeq_epi16(value, needle) : _mm_cmpeq_epi32(value, needle);
        const unsigned mask = _mm_movemask_epi8(cmp);
        if (mask)
            return pos + (first_set(mask) >> (kind >> 1));
        pos += lanes;
    }
#endif
    while (pos < size && PyUnicode_READ(kind, data, pos) != '<')
        ++pos;
    return pos;
}

static Py_ssize_t close_ticks(Py_ssize_t ticks, Py_ssize_t run)
{
    return !ticks ? run : ticks == run ? 0 : ticks;
}

static PyObject *backticks(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError, "backticks requires text and initial run");
        return NULL;
    }
    PyObject *text = args[0];
    if (!PyUnicode_Check(text)) {
        PyErr_SetString(PyExc_TypeError, "text must be a string");
        return NULL;
    }
    Py_ssize_t ticks = PyNumber_AsSsize_t(args[1], PyExc_OverflowError);
    if (ticks == -1 && PyErr_Occurred())
        return NULL;
    if (ticks < 0) {
        PyErr_SetString(PyExc_ValueError, "ticks must be nonnegative");
        return NULL;
    }
    if (PyUnicode_READY(text) < 0)
        return NULL;
    const void *data = PyUnicode_DATA(text);
    const int kind = PyUnicode_KIND(text);
    const Py_ssize_t size = PyUnicode_GET_LENGTH(text);
    Py_ssize_t pos = PyUnicode_FindChar(text, '`', 0, size, 1), run = 0;
    if (pos < 0)
        return pos == -2 ? NULL : PyLong_FromSsize_t(ticks);
#ifdef TEXT_SSE2
    const unsigned shift = kind >> 1;
    const Py_ssize_t lanes = 16 >> shift;
    const __m128i needle = kind == 1 ? _mm_set1_epi8('`') :
                          kind == 2 ? _mm_set1_epi16('`') : _mm_set1_epi32('`');
    while (size - pos >= lanes) {
        const __m128i value = _mm_loadu_si128((const __m128i *)((const char *)data + pos * kind));
        const __m128i cmp = kind == 1 ? _mm_cmpeq_epi8(value, needle) :
                            kind == 2 ? _mm_cmpeq_epi16(value, needle) : _mm_cmpeq_epi32(value, needle);
        unsigned mask = _mm_movemask_epi8(cmp);
        if (mask == 0xffff) {
            run += lanes;
        } else {
            unsigned used = 0;
            while (mask) {
                const unsigned gap = first_set(mask);
                if (gap) {
                    ticks = close_ticks(ticks, run);
                    run = 0;
                    mask >>= gap;
                    used += gap;
                }
                const unsigned count = first_set(~mask);  /* upper 16 bits are always set */
                run += count >> shift;
                mask >>= count;
                used += count;
            }
            if (used < 16) {
                ticks = close_ticks(ticks, run);
                run = 0;
            }
        }
        pos += lanes;
    }
#endif
    while (pos < size) {
        if (PyUnicode_READ(kind, data, pos++) == '`')
            ++run;
        else if (run) {
            ticks = close_ticks(ticks, run);
            run = 0;
        }
    }
    return PyLong_FromSsize_t(close_ticks(ticks, run));
}

/* 0: different, 1: prefix at the end of the input, 2: whole literal. */
static int literal(const void *data, int kind, Py_ssize_t pos,
                   Py_ssize_t size, const char *tag)
{
    while (*tag && pos < size) {
        if (PyUnicode_READ(kind, data, pos++) != (unsigned char)*tag++)
            return 0;
    }
    return *tag ? 1 : 2;
}

static PyObject *parameter_end(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (nargs != 2 && nargs != 3) {
        PyErr_SetString(PyExc_TypeError, "parameter_end requires text, final and optional start");
        return NULL;
    }
    PyObject *text = args[0];
    if (!PyUnicode_Check(text)) {
        PyErr_SetString(PyExc_TypeError, "text must be a string");
        return NULL;
    }
    const int final = PyObject_IsTrue(args[1]);
    if (final < 0)
        return NULL;
    const Py_ssize_t start = nargs == 3 ? PyNumber_AsSsize_t(args[2], PyExc_OverflowError) : 0;
    if (start == -1 && PyErr_Occurred())
        return NULL;
    if (PyUnicode_READY(text) < 0)
        return NULL;
    const void *data = PyUnicode_DATA(text);
    const int kind = PyUnicode_KIND(text);
    const Py_ssize_t size = PyUnicode_GET_LENGTH(text);
    if (start < 0 || start > size) {
        PyErr_SetString(PyExc_ValueError, "start must be within text");
        return NULL;
    }
    Py_ssize_t pos = start;
    while ((pos = tag_start(data, kind, pos, size)) < size) {
        if (literal(data, kind, pos, size, "</parameter>") == 2) {
            Py_ssize_t after = pos + 12;
            while (after < size && Py_UNICODE_ISSPACE(PyUnicode_READ(kind, data, after)))
                ++after;
            const int param = literal(data, kind, after, size, "<parameter=");
            const int func = literal(data, kind, after, size, "</function>");
            if (param == 2 || func == 2)
                return PyLong_FromSsize_t(pos);
            if (param == 1 || func == 1)
                return PyLong_FromSsize_t(final ? pos : -2);
            pos = after;
        } else {
            ++pos;
        }
    }
    return PyLong_FromLong(-1);
}

/* Generated markers are non-BMP characters: reject four nonmarkers per load. */
static Py_ssize_t marker_start(const void *data, int kind, Py_ssize_t pos,
                               Py_ssize_t size, Py_UCS4 low, Py_UCS4 high)
{
#ifdef TEXT_SSE2
    if (kind == 4) {
        const __m128i lower = _mm_set1_epi32(low), upper = _mm_set1_epi32(high);
        while (size - pos >= 4) {
            const __m128i value = _mm_loadu_si128((const __m128i *)((const Py_UCS4 *)data + pos));
            const unsigned mask = (~_mm_movemask_epi8(_mm_or_si128(
                _mm_cmpgt_epi32(lower, value), _mm_cmpgt_epi32(value, upper)))) & 0xffff;
            if (mask)
                return pos + first_set(mask) / 4;
            pos += 4;
        }
    }
#endif
    while (pos < size) {
        const Py_UCS4 ch = PyUnicode_READ(kind, data, pos);
        if (ch >= low && ch <= high)
            break;
        ++pos;
    }
    return pos;
}

static PyObject *restore_markers(PyObject *self, PyObject *args)
{
    PyObject *text, *marks, *key, *tag;
    if (!PyArg_ParseTuple(args, "UO!:restore_markers", &text, &PyDict_Type, &marks))
        return NULL;
    if (PyUnicode_READY(text) < 0)
        return NULL;
    Py_UCS4 low = 0x110000, high = 0;
    Py_ssize_t pos = 0;
    while (PyDict_Next(marks, &pos, &key, &tag)) {
        if (!PyUnicode_Check(key) || !PyUnicode_Check(tag)) {
            PyErr_SetString(PyExc_TypeError, "markers and replacements must be strings");
            return NULL;
        }
        if (PyUnicode_READY(key) < 0 || PyUnicode_READY(tag) < 0)
            return NULL;
        if (PyUnicode_GET_LENGTH(key) != 1) {
            PyErr_SetString(PyExc_ValueError, "markers must be single characters");
            return NULL;
        }
        const Py_UCS4 ch = PyUnicode_READ_CHAR(key, 0);
        if (ch < low) low = ch;
        if (ch > high) high = ch;
    }
    PyObject *spans = PyList_New(0);
    if (!spans)
        return NULL;
    if (low > PyUnicode_MAX_CHAR_VALUE(text))
        return Py_BuildValue("(ON)", text, spans);
    _PyUnicodeWriter writer;
    _PyUnicodeWriter_Init(&writer);
    writer.overallocate = 1;
    const void *data = PyUnicode_DATA(text);
    const int kind = PyUnicode_KIND(text);
    const Py_ssize_t size = PyUnicode_GET_LENGTH(text);
    pos = 0;
    Py_ssize_t at = 0;
    while ((at = marker_start(data, kind, at, size, low, high)) < size) {
        key = PyUnicode_FromOrdinal(PyUnicode_READ(kind, data, at));
        if (!key)
            goto error;
        tag = PyDict_GetItemWithError(marks, key);
        Py_DECREF(key);
        if (!tag) {
            if (PyErr_Occurred())
                goto error;
            ++at;
            continue;
        }
        if (_PyUnicodeWriter_WriteSubstring(&writer, text, pos, at) < 0)
            goto error;
        const Py_ssize_t begin = writer.pos;
        if (_PyUnicodeWriter_WriteStr(&writer, tag) < 0)
            goto error;
        PyObject *span = Py_BuildValue("(nn)", begin, writer.pos);
        if (!span)
            goto error;
        const int appended = PyList_Append(spans, span);
        Py_DECREF(span);
        if (appended < 0)
            goto error;
        pos = ++at;
    }
    if (_PyUnicodeWriter_WriteSubstring(&writer, text, pos, size) < 0)
        goto error;
    text = _PyUnicodeWriter_Finish(&writer);
    if (!text)
        goto error;
    return Py_BuildValue("(NN)", text, spans);
error:
    _PyUnicodeWriter_Dealloc(&writer);
    Py_DECREF(spans);
    return NULL;
}

static PyMethodDef methods[] = {
    {"backticks", (PyCFunction)(void (*)(void))backticks, METH_FASTCALL, "Open backtick run after (text, initial run)."},
    {"parameter_end", (PyCFunction)(void (*)(void))parameter_end, METH_FASTCALL,
     "Parameter terminator in (text, final, start=0), as an absolute offset."},
    {"restore_markers", restore_markers, METH_VARARGS, "Restore single-character markers in one forward pass."},
    {NULL, NULL, 0, NULL}
};
static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT, "_text_scan", "Bounded SIMD text scans.", -1, methods
};
PyMODINIT_FUNC PyInit__text_scan(void) { return PyModule_Create(&module); }
