#pragma once

// Счётчик вызовов XQueryKeymap в тестовом исполняемом файле.
unsigned int keyboardQueryCount();

// Задать состояние клавиши в ответе тестовой подмены XQueryKeymap.
void setKeyboardKeyHeld(unsigned char keyCode, bool held);
// Вернуть чтение настоящего состояния клавиатуры X11.
void useNativeKeyboardInput();
