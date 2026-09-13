#pragma once

namespace bag {

struct Lesson {
    const char* title;
    const char* body;
};

struct Experiment {
    const char* title;
    const char* prompt;
    const char* equation;
};

const Lesson& lessonAt(int index);
const Experiment& experimentAt(int index);
int lessonCount();
int experimentCount();

} // namespace bag
