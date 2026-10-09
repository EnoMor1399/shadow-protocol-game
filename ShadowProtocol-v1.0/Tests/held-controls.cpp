#include "SPHeldLeanState.h"
#include <cassert>
#include <iostream>

int main()
{
    FSPHeldLeanState state;
    assert(state.Value() == 0.f);
    state.Left = true;
    assert(state.Value() == -1.f);
    state.Right = true;
    assert(state.Value() == 0.f);
    state.Left = false;
    assert(state.Value() == 1.f); // right must survive left key-up
    state.Right = false;
    assert(state.Value() == 0.f);
    state.Right = true;
    state.Left = true;
    state.Right = false;
    assert(state.Value() == -1.f); // same behavior in reverse order
    state.Reset(); // opening/closing a modal releases every held direction
    assert(state.Value() == 0.f);
    state.Left = false; // delayed key-up after a flush remains neutral
    state.Right = false;
    assert(state.Value() == 0.f);
    state.Right = true;
    assert(state.Value() == 1.f); // fresh input works after menu closes
    std::cout << "Held lean: overlapping keys, release ordering and modal reset passed\n";
}
