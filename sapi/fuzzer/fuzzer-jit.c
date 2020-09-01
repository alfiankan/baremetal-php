/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Nikita Popov <nikic@php.net>                                |
   +----------------------------------------------------------------------+
 */

#include "fuzzer-execute-common.h"
#include "zend_exceptions.h"

static void opcache_invalidate(void) {
	steps_left = MAX_STEPS;
	zend_exception_save();
	zval retval, func, args[2];
	ZVAL_STRING(&func, "opcache_invalidate");
	ZVAL_STRING(&args[0], "/fuzzer.php");
	ZVAL_TRUE(&args[1]);
	call_user_function(CG(function_table), NULL, &func, &retval, 2, args);
	ZEND_ASSERT(Z_TYPE(retval) == IS_TRUE);
	zval_ptr_dtor(&args[0]);
	zval_ptr_dtor(&retval);
	zval_ptr_dtor(&func);
	zend_exception_restore();
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
	if (Size > MAX_SIZE) {
		/* Large inputs have a large impact on fuzzer performance,
		 * but are unlikely to be necessary to reach new codepaths. */
		return 0;
	}

	zend_string *jit_option = zend_string_init("opcache.jit", sizeof("opcache.jit") - 1, 1);

	/* First run without JIT to determine whether we bail out. We should not run JITed code if
	 * we bail out here, as the JIT code may loop infinitely. */
	steps_left = MAX_STEPS;
	bailed_out = false;
	zend_alter_ini_entry_chars(
		jit_option, "off", sizeof("off")-1, PHP_INI_USER, PHP_INI_STAGE_RUNTIME);
	fuzzer_do_request_from_buffer(
		"/fuzzer.php", (const char *) Data, Size, /* execute */ 1, opcache_invalidate);

	if (!bailed_out) {
		steps_left = MAX_STEPS;
		zend_alter_ini_entry_chars(jit_option,
			"function", sizeof("function")-1, PHP_INI_USER, PHP_INI_STAGE_RUNTIME);
		fuzzer_do_request_from_buffer(
			"/fuzzer.php", (const char *) Data, Size, /* execute */ 1, opcache_invalidate);
	}

	zend_string_release(jit_option);

	return 0;
}

char *get_opcache_path(void) {
	// TODO: Make this more general.
	char *opcache_path = "modules/opcache.so";
	return realpath(opcache_path, NULL);
}

int LLVMFuzzerInitialize(int *argc, char ***argv) {
	char *opcache_path = get_opcache_path();
	assert(opcache_path && "Failed to determine opcache path");

	char ini_buf[256];
	snprintf(ini_buf, sizeof(ini_buf),
		"zend_extension=%s\n"
		"opcache.validate_timestamps=0\n"
		"opcache.file_update_protection=0\n"
		"opcache.jit_buffer_size=512M",
		opcache_path);
	free(opcache_path);
	fuzzer_init_php_for_execute(ini_buf);
	return 0;
}
