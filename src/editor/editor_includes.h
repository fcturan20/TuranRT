#pragma once
#include <turanapi/threadingsys_tapi.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>

enum class result_editor {
	SUCCESS,
	CRASH,
	FAIL
};
void printer_editor(result_editor result, const char* log);