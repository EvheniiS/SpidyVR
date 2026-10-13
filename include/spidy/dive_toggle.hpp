#pragma once
namespace spidy {
// The dive, as the game has it (FallDive): a press in free flight starts it
// and the player falls under the game's own free-fall gravity, faster and
// faster (SwingConfig::diveGravity); a web that then attaches swings on with
// that speed (fallCarry). The press is the right thumbstick's click
// (StickPress keeps it apart from clicking both). Another press, a web or
// landing ends it.
class DiveToggle {
  public:
    enum class Change { none, started, ended };
    // pressed: a counted press. free: Spidy flies the player in the air with
    // no web attached.
    Change update(bool pressed, bool free) {
        if (diving_ && (pressed || !free)) {
            diving_ = false;
            return Change::ended;
        }
        if (!pressed || !free || diving_)
            return Change::none;
        diving_ = true;
        return Change::started;
    }
    bool diving() const {
        return diving_;
    }

  private:
    bool diving_{};
};
} // namespace spidy
