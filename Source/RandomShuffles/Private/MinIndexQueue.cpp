/**
 * Copyright Anthony Arnold (RK4XYZ), 2023.
 */
#include "MinIndexQueue.h"

namespace RandomShuffles {

MinIndexQueue::MinIndexQueue(std::size_t MaxSize) : Priorities(MaxSize), 
    Queue(Compare {this})
{
}

float MinIndexQueue::MinimumKey() const {
    return Priorities[Queue.top()];
}

std::size_t MinIndexQueue::ExtractMin() {
    auto top = Queue.top();
    Queue.pop();
    return top;
}

void MinIndexQueue::Push(float Priority, std::size_t Index) {
    Priorities[Index] = Priority;
    Queue.push(Index);
}

std::size_t MinIndexQueue::Size() const {
    return Queue.size();
}

bool MinIndexQueue::Compare::operator()(std::size_t left, std::size_t right) const {
    return Queue->Priorities[left] > Queue->Priorities[right];
}

}