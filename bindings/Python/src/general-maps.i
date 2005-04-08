// -*- C -*-

/* It would be nice to use python time types ? NCP */
%apply long { time_t }
%apply unsigned long { recordid_t };

// ------------------------------------------------------
// Proper pi_buffer_t management, specifically designed
// as to not leak in case of early termination (hence the
// freearg part)
// ------------------------------------------------------
%typemap (in,numinputs=0) (pi_buffer_t *) {
	$1 = pi_buffer_new(0xFFFF);
}

%typemap (freearg) (pi_buffer_t *) {
	if ($1) {
		pi_buffer_free($1);
	}
}

%typemap (python,argout) (pi_buffer_t *) {
	if ($1) {
		PyObject *o1 = Py_BuildValue("s#", $1->data, $1->used);
		$result = t_output_helper($result, o1);
	}
}

%typemap (in,numinputs=0) (size_t *OUTBUFLEN) (size_t outbuflen) {
  outbuflen = 0xFFFF;
  $1 = &outbuflen;
}

// a generic 4-character string type, for use as a type or creator ID
%typemap (python,in) unsigned long STR4 {
	if (!($input) || ($input == Py_None)) {
		$1 = 0;
	} else {
		if (!PyString_Check($input) || (PyString_Size($input) != 4)) {
			PyErr_SetString(PyExc_ValueError, "argument must be a 4-character string");
			return 0;
		}
		$1 = makelong(PyString_AsString($input));
	}
}

%typemap (python,in) long STR4 {
	if (!($input) || ($input == Py_None)) {
		$1 = 0;
	} else {
		if (!PyString_Check($input) || (PyString_Size($input) != 4)) {
			PyErr_SetString(PyExc_ValueError, "argument must be a 4-character string");
			return 0;
		}
		$1 = makelong(PyString_AsString($input));
	}
}

%typemap (python,argout) unsigned long *OUTSTR4 {
	if ($1) {
		PyObject *o = PyString_FromStringAndSize(printlong(*$1), 4);
		$result = t_output_helper($result, o);
	}
}

%typemap (in,numinputs=0) unsigned long *OUTSTR4 (unsigned long temp) {
	$1 = &temp;
}

