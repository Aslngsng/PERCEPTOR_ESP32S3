/*
 PERCEPTOR - Phase 1: navigation and Learning Mode prototype
 Target: ESP32-S3-DevKitC-1 (Arduino-ESP32)

 SAFE TEST BUILD: NO ACTUATOR OUTPUTS. The custom 96-coil selection
 network, current limit, safe pulse lengths and H-bridge remain unverified.
 Voice prompts and Braille patterns are printed on Serial only.

 Temporary user-interface decisions to test:
 - Next / Back scroll options in menus; Speak confirms menu selection.
 - Long Back (2 s) returns to main menu from any mode.
 - Learning: Next/Back select item, Repeat repeats prompt, Speak ignored.
 - The final Next in Learning says 'Lesson completed' and goes to menu.
 - Reading and Quiz are placeholders (not functional yet).

 Serial Monitor (115200 baud): n=Next, b=Back, r=Repeat,
 s=Speak/Confirm, x=long-Back, h=help.
 Buttons connect between GPIO and GND (internal pullups).
*/
#include <Arduino.h>

constexpr uint8_t PIN_NEXT = 14;
constexpr uint8_t PIN_BACK = 15;
constexpr uint8_t PIN_REPEAT = 16;
constexpr uint8_t PIN_SPEAK = 17;
constexpr uint32_t DEBOUNCE_MS = 35;
constexpr uint32_t LONG_PRESS_MS = 2000;

enum class Button : uint8_t { NEXT, BACK, REPEAT, SPEAK, LONG_BACK, NONE };
enum class State : uint8_t { MENU, DIFFICULTY, LEARNING, READING, QUIZ };
enum class Mode : uint8_t { LEARNING, READING, QUIZ };
enum class Difficulty : uint8_t { EASY, MEDIUM, HARD };

struct Key {
  uint8_t gpio;
  bool stableHigh = true;
  bool lastRawHigh = true;
  uint32_t rawChangedAt = 0;
  uint32_t pressedAt = 0;
  bool longSent = false;
};
Key keys[4] = {{PIN_NEXT}, {PIN_BACK}, {PIN_REPEAT}, {PIN_SPEAK}};

State state = State::MENU;
Mode selectedMode = Mode::LEARNING;
Difficulty difficulty = Difficulty::EASY;
uint8_t menuIndex = 0;
uint8_t difficultyIndex = 0;
size_t lessonIndex = 0;

// Deliberately limited to items that fit in 16 Grade 1 Braille cells.
// For this prototype, only lowercase A-Z and ASCII spaces are supported.
// Numbers, punctuation and capital indicators will be added in the translator phase.
const char *const easyItems[] = {
  "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
  "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"
};

// 3-letter English word
const char *const mediumItems[] = {
  "ace", "act", "add", "age", "ago", "aid", "aim", "air", "ale", "all", "and", "ant", "any", "ape", "app", "apt", "arc", "are", "ark", "arm", "art", "ash", "ask", "ate", "awe", "awl", "axe",
  "bad", "bag", "ban", "bar", "bat", "bay", "bed", "bee", "beg", "bet", "bib", "bid", "big", "bin", "bit", "bob", "bog", "bow", "box", "boy", "bud", "bug", "bum", "bun", "bus", "but", "buy", "bye",
  "cab", "cad", "can", "cap", "car", "cat", "caw", "cob", "cod", "cog", "cop", "cot", "cow", "coy", "cry", "cub", "cue", "cup", "cut",
  "dad", "dam", "day", "den", "dew", "did", "die", "dig", "dim", "dip", "dog", "don", "dot", "dry", "dub", "due", "dug", "dun", "duo", "dye",
  "ear", "eat", "ebb", "eel", "egg", "ego", "elf", "elk", "elm", "end", "era", "eve", "ewe", "eye",
  "fab", "fan", "far", "fat", "fax", "fay", "fed", "fee", "few", "fig", "fin", "fir", "fit", "fix", "fly", "fog", "for", "fox", "fry", "fun", "fur",
  "gab", "gad", "gag", "gal", "gap", "gas", "gay", "gel", "gem", "get", "gig", "gin", "gnu", "gob", "god", "got", "gum", "gun", "gut", "guy", "gym",
  "had", "hag", "ham", "has", "hat", "hay", "hem", "hen", "her", "hew", "hey", "hid", "him", "hip", "his", "hit", "hob", "hoe", "hog", "hop", "hot", "how", "hub", "hug", "hum", "hut",
  "ice", "icy", "ill", "ink", "inn", "ion", "its", "ivy"
  "jam", "jar", "jaw", "jay", "jet", "jew", "jig", "job", "jog", "joy", "jug", "jut",
  "keg", "ken", "key", "kid", "kin", "kit"
  "lab", "lad", "lag", "lap", "law", "lay", "led", "leg", "let", "lid", "lie", "lip", "lit", "lob", "log", "lot", "low", "lug",
  "mad", "man", "map", "mat", "maw", "may", "men", "met", "mid", "mix", "mob", "mom", "mud", "mug", "mum",
  "nag", "nap", "net", "new", "nil", "nip", "nit", "nod", "nor", "not", "now", "nun", "nut",
  "oaf", "oak", "oar", "oat", "odd", "ode", "off", "oft", "oil", "old", "ole", "one", "opt", "orb", "ore", "our", "out", "owe", "owl", "own",
  "pad", "pal", "pan", "pap", "par", "pat", "paw", "pay", "pea", "peg", "pen", "pep", "per", "pet", "pew", "pie", "pig", "pin", "pit", "ply", "pod", "pop", "pot", "pow", "pub", "pug", "pun", "pup", "put",
  "qua", "que"
  "rad", "rag", "ram", "ran", "rap", "rat", "raw", "ray", "red", "ref", "rep", "rib", "rid", "rig", "rim", "rip", "rob", "rod", "roe", "rot", "row", "rub", "rug", "rum", "run", "rut",
  "sad", "sag", "sap", "sat", "saw", "say", "sea", "see", "set", "sew", "she", "shy", "sib", "sin", "sip", "sir", "sis", "sit", "six", "ski", "sky", "sly", "sob", "sod", "son", "sop", "sow", "soy", "spa", "spy", "sub", "sue", "sum", "sun",
  "tab", "tad", "tag", "tan", "tap", "tar", "tat", "tax", "tea", "ten", "the", "thy", "tie", "tin", "tip", "toe", "ton", "too", "top", "toy", "try", "tub", "tug", "two",
  "ugh", "use",
  "van", "vat", "vet", "vex", "via", "vie", "vim", "vow"
  "wad", "wag", "war", "was", "wax", "way", "web", "wed", "wee", "wet", "who", "why", "wig", "win", "wit", "woe", "won", "wow", "wry"
  "xis",
  "yak", "yam", "yap", "yar", "yaw", "yay", "yes", "yet", "yew", "yin", "yip", "you",
  "zap", "zen", "zip", "zit", "zoo"
  };
 

// English phrase
const char *const hardItems[] = {
  "all done",
  "are you ok",
  "ask for help",
  "at my desk",
  "be nice",
  "big book",
  "blue pen",
  "bring it",
  "bye bye",
  "can i go",
  "can i play",
  "come here", "come in",
  "clean up",
  "color it",
  "do my work",
  "do it",
  "do you know",
  "draw a cat",
  "excuse me",
  "eat lunch",
  "end of day",
  "feel sick",
  "find my bag",
  "follow me",
  "for my mom",
  "fun day",
  "get in line",
  "get my book",
  "good job",
  "good morning",
  "good night",
  "good work",
  "hands up",
  "have a seat",
  "help me",
  "here you go",
  "hi there",
  "how are you",
  "i am done",
  "i am fine",
  "i can read",
  "i can try",
  "i like it",
  "i love you",
  "i need help",
  "i want it",
  "i will try",
  "join us",
  "just wait",
  "keep going",
  "kind words",
  "let me see",
  "let us go",
  "line up",
  "look at me",
  "look here",
  "lunch time",
  "may i go",
  "may i play",
  "my book",
  "my turn",
  "my name is",
  "no thank you",
  "nice work",
  "not yet",
  "open it",
  "our class",
  "over here",
  "pass it",
  "pick me",
  "please help",
  "please stop",
  "put it back",
  "quiet please",
  "read it",
  "ready to go",
  "raise your hand",
  "right here",
  "same here",
  "say it",
  "see you",
  "sit down",
  "slow down",
  "so sorry",
  "stand up",
  "stop it",
  "show me",
  "school time",
  "thank you",
  "take turns",
  "that is mine",
  "this is fun",
  "try again",
  "turn it in",
  "up here",
  "use a pen",
  "very good",
  "wait for me",
  "wash my hands",
  "what is it",
  "where is it",
  "who is next",
  "well done",
  "write it",
  "yes please",
  "you can go",
  "you did it",
  "you and me",
  "zip it"
  };

struct Lesson { const char *const *items; size_t count; };
Lesson getLesson() {
  switch (difficulty) {
    case Difficulty::EASY: return {easyItems, sizeof(easyItems)/sizeof(easyItems[0])};
    case Difficulty::MEDIUM: return {mediumItems, sizeof(mediumItems)/sizeof(mediumItems[0])};
    case Difficulty::HARD: return {hardItems, sizeof(hardItems)/sizeof(hardItems[0])};
  }
  return {easyItems, sizeof(easyItems)/sizeof(easyItems[0])};
}

// Dot order in bits 0..5 is dot 1..6. Zero is a blank cell.
// Verified standard uncontracted alphabet dot sets; no Grade 2 contractions.
const uint8_t alphabetDots[26] = {
  0x01,0x03,0x09,0x19,0x11,0x0B,0x1B,0x13,0x0A,0x1A,
  0x05,0x07,0x0D,0x1D,0x15,0x0F,0x1F,0x17,0x0E,0x1E,
  0x25,0x27,0x3A,0x2D,0x3D,0x35
};
uint8_t desiredCells[16] = {};
uint8_t lastCommandedCells[16] = {};

void announce(const String &message) {
  // Placeholder for recorded audio playback via MAX98357A / MicroSD.
  Serial.print("[AUDIO MOCK] "); Serial.println(message);
}

void showBrailleMock(const char *item) {
  memset(desiredCells, 0, sizeof(desiredCells));
  size_t n = strlen(item);
  if (n > 16) { announce("Unsupported lesson length"); return; }
  for (size_t i = 0; i < n; ++i) {
    char c = item[i];
    if (c >= 'a' && c <= 'z') desiredCells[i] = alphabetDots[c - 'a'];
    else if (c != ' ') { announce("Unsupported character"); return; }
  }
  Serial.print("[BRAILLE MOCK] Text: "); Serial.println(item);
  for (size_t i = 0; i < 16; ++i) {
    uint8_t changes = desiredCells[i] ^ lastCommandedCells[i];
    if (changes) {
      Serial.printf("  cell %u: desired=0x%02X changed-dot-mask=0x%02X\n",
                    unsigned(i + 1), unsigned(desiredCells[i]), unsigned(changes));
    }
  }
  // This is a SOFTWARE SIMULATION ONLY. It does not establish real dot positions.
  memcpy(lastCommandedCells, desiredCells, sizeof(lastCommandedCells));
}

const char *modeName(uint8_t i) { return i == 0 ? "Learning" : i == 1 ? "Reading" : "Quiz"; }
const char *difficultyName(uint8_t i) { return i == 0 ? "Easy" : i == 1 ? "Medium" : "Hard"; }

void printMenu() {
  Serial.println("\n-- Main menu --");
  Serial.printf("Selected: %s. Press Next/Back, then Speak to confirm.\n",modeName(menuIndex));
  announce(String(modeName(menuIndex)) + " mode");
}
void enterMenu() {
  state = State::MENU;
  printMenu();
}
void enterDifficulty() {
  state = State::DIFFICULTY;
  difficultyIndex = 0;
  announce("Select difficulty");
  announce(difficultyName(difficultyIndex));
}
void renderLesson() {
  Lesson lesson = getLesson();
  if (lessonIndex >= lesson.count) return;
  const char *item = lesson.items[lessonIndex];
  showBrailleMock(item);
  announce(item);
  Serial.printf("Lesson item %u of %u\n", unsigned(lessonIndex+1), unsigned(lesson.count));
}
void startSelectedMode() {
  if (selectedMode == Mode::LEARNING) {
    state = State::LEARNING;
    lessonIndex = 0;
    announce(String("Learning Mode. ") + difficultyName(difficultyIndex));
    renderLesson();
  } else if (selectedMode == Mode::READING) {
    state = State::READING;
    announce("Reading Mode");
    Serial.println("[PENDING] BLE upload, MicroSD and Grade 1 pagination are not built yet.");
  } else {
    state = State::QUIZ;
    announce(String("Quiz Mode. ") + difficultyName(difficultyIndex));
    Serial.println("[PENDING] Offline speech recognition and attempt limits are not built yet.");
  }
}

void onButton(Button button) {
  if (button == Button::NONE) return;
  if (button == Button::LONG_BACK) { announce("Returning to main menu"); enterMenu(); return; }
  switch (state) {
    case State::MENU:
      if (button == Button::NEXT) { menuIndex = (menuIndex + 1) % 3; printMenu(); }
      else if (button == Button::BACK) { menuIndex = (menuIndex + 2) % 3; printMenu(); }
      else if (button == Button::REPEAT) printMenu();
      else if (button == Button::SPEAK) {
        selectedMode = static_cast<Mode>(menuIndex);
        if (selectedMode == Mode::READING) startSelectedMode();
        else enterDifficulty();
      }
      break;
    case State::DIFFICULTY:
      if (button == Button::NEXT) { difficultyIndex = (difficultyIndex + 1) % 3; announce(difficultyName(difficultyIndex)); }
      else if (button == Button::BACK) { difficultyIndex = (difficultyIndex + 2) % 3; announce(difficultyName(difficultyIndex)); }
      else if (button == Button::REPEAT) announce(difficultyName(difficultyIndex));
      else if (button == Button::SPEAK) { difficulty = static_cast<Difficulty>(difficultyIndex); startSelectedMode(); }
      break;
    case State::LEARNING:
      if (button == Button::NEXT) {
        Lesson lesson = getLesson();
        if (lessonIndex + 1 >= lesson.count) { announce("Lesson completed"); enterMenu(); }
        else { ++lessonIndex; renderLesson(); }
      } else if (button == Button::BACK) {
        if (lessonIndex == 0) announce("Beginning of lesson");
        else { --lessonIndex; renderLesson(); }
      } else if (button == Button::REPEAT) announce(getLesson().items[lessonIndex]);
      // SPEAK deliberately unused in Learning Mode.
      break;
    case State::READING:
      Serial.println("[PENDING] Reading Mode logic not connected yet.");
      break;
    case State::QUIZ:
      Serial.println("[PENDING] Quiz Mode logic not connected yet.");
      break;
  }
}

Button pollButtons() {
  const uint32_t now = millis();
  for (uint8_t i = 0; i < 4; ++i) {
    Key &k = keys[i];
    bool rawHigh = digitalRead(k.gpio) == HIGH;
    if (rawHigh != k.lastRawHigh) { k.lastRawHigh = rawHigh; k.rawChangedAt = now; }
    if (now - k.rawChangedAt < DEBOUNCE_MS || rawHigh == k.stableHigh) {
      if (i == 1 && !k.stableHigh && !k.longSent && now-k.pressedAt >= LONG_PRESS_MS) {
        k.longSent = true;
        return Button::LONG_BACK;
      }
      continue;
    }
    k.stableHigh = rawHigh;
    if (!rawHigh) { k.pressedAt = now; k.longSent = false; }
    else if (!k.longSent) return static_cast<Button>(i);
  }
  return Button::NONE;
}

Button pollSerial() {
  if (!Serial.available()) return Button::NONE;
  char ch = static_cast<char>(Serial.read());
  switch (ch) {
    case 'n': case 'N': return Button::NEXT;
    case 'b': case 'B': return Button::BACK;
    case 'r': case 'R': return Button::REPEAT;
    case 's': case 'S': return Button::SPEAK;
    case 'x': case 'X': return Button::LONG_BACK;
    case 'h': case 'H':
      Serial.println("Commands: n=Next b=Back r=Repeat s=Speak/Confirm x=Long Back");
      return Button::NONE;
  }
  return Button::NONE;
}

void setup() {
  Serial.begin(115200);
  for (auto &key : keys) pinMode(key.gpio, INPUT_PULLUP);
  // NO H-bridge or decoder GPIOs initialized in Phase 1.
  Serial.println("PERCEPTOR Phase 1 - safe navigation prototype");
  Serial.println("Commands: n=Next b=Back r=Repeat s=Speak/Confirm x=Long Back");
  enterMenu();
}

void loop() {
  Button physical = pollButtons();
  if (physical != Button::NONE) onButton(physical);
  Button simulated = pollSerial();
  if (simulated != Button::NONE) onButton(simulated);
}
