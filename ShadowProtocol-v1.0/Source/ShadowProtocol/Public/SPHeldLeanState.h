#pragma once

// Shared by the pawn and standalone regression tests. Both held means neutral;
// releasing either key restores the remaining direction rather than cancelling it.
struct FSPHeldLeanState
{
    bool Left = false;
    bool Right = false;
    float Value() const { return (Right ? 1.f : 0.f) - (Left ? 1.f : 0.f); }
    void Reset() { Left = false; Right = false; }
};
