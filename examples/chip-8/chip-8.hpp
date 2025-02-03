#ifndef CHIP_8_HPP
#define CHIP_8_HPP

#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <vector>

static inline const uint8_t font[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

static constexpr uint8_t A = 10;
static constexpr uint8_t B = 11;
static constexpr uint8_t C = 12;
static constexpr uint8_t D = 13;
static constexpr uint8_t E = 14;
static constexpr uint8_t F = 15;

struct instruction_t {
  uint16_t higher_byte() { return _inst >> 8; }
  uint16_t lower_byte() { return _inst & 0x00FF; }

  uint16_t first_nibble() { return higher_byte() >> 4; }
  uint16_t second_nibble() { return higher_byte() & 0x0F; }
  uint16_t third_nibble() { return lower_byte() >> 4; }
  uint16_t fourth_nibble() { return lower_byte() & 0x0F; }

  uint16_t address() {
    return (second_nibble() << 8) | ((third_nibble() << 4) | fourth_nibble());
  }

  uint16_t _inst;
};

inline std::ostream &operator<<(std::ostream &o, instruction_t instruction) {
  o << std::setw(3) << std::hex << instruction.first_nibble() << ' ' << std::hex
    << std::setw(3) << instruction.second_nibble() << ' ' << std::setw(3)
    << std::hex << instruction.third_nibble() << ' ' << std::setw(3) << std::hex
    << instruction.fourth_nibble() << std::dec;
  return o;
}

template <size_t width, size_t height> struct chip8_t {
  uint8_t memory[4 * 1024];
  uint16_t pc;
  uint16_t I;
  union {
    uint8_t V[16];
    struct {
      uint8_t V0;
      uint8_t V1;
      uint8_t V2;
      uint8_t V3;
      uint8_t V4;
      uint8_t V5;
      uint8_t V6;
      uint8_t V7;
      uint8_t V8;
      uint8_t V9;
      uint8_t VA;
      uint8_t VB;
      uint8_t VC;
      uint8_t VD;
      uint8_t VE;
      uint8_t VF;
    };
  };
  uint8_t delay;
  uint8_t sound;
  uint32_t fb[width * height];
  bool keys[16];
  std::stack<uint16_t> stack{};

  instruction_t instruction{};

  static constexpr uint16_t font_address = 0x0050;
  static constexpr uint16_t start_of_rom_address = 0x0200;

  uint32_t on_color = 0xFF00FFFF;
  uint32_t off_color = 0xFF000000;

  static chip8_t create() {
    chip8_t chip8{};
    std::memset(chip8.memory, 0, sizeof(chip8.memory));
    chip8.pc = 0;
    chip8.I = 0;
    std::memset(chip8.V, 0, sizeof(chip8.V));
    chip8.delay = 0;
    chip8.sound = 0;
    for (auto &pixel : chip8.fb)
      pixel = chip8.off_color;
    std::memset(chip8.keys, 0, sizeof(chip8.keys));
    chip8.stack = {};
    chip8.instruction = {};
    return chip8;
  }

  void throw_unknown_instruction(instruction_t instruction) {
    std::stringstream ss{};
    ss << "Unkown instruction: " << instruction << '\n';
    throw std::runtime_error(ss.str());
  }

  void upload_font(const uint8_t *font, size_t size) {
    std::memcpy(memory + font_address, font, size);
  }

  void upload_rom(const uint8_t *obj, size_t size) {
    std::memcpy(memory + start_of_rom_address, obj, size);
    // the program counter points to the first instruction in the rom
    pc = start_of_rom_address;
  }

  void press_key(uint8_t key_id) {
    assert(key_id < 16);
    keys[key_id] = true;
  }

  void release_key(uint8_t key_id) {
    assert(key_id < 16);
    keys[key_id] = false;
  }

  void tick() {
    if (delay)
      delay--;
    if (sound)
      sound--;
  }

  instruction_t fetch() {
    uint8_t hi = memory[pc++];
    uint8_t lo = memory[pc++];
    uint16_t inst = (hi << 8) | lo;
    instruction = {._inst = inst};
    return instruction;
  }

  instruction_t peek_next_instruction() {
    uint8_t hi = memory[pc];
    uint8_t lo = memory[pc + 1];
    uint16_t inst = (hi << 8) | lo;
    return {._inst = inst};
  }

  std::tuple<std::string, std::string, std::string>
  disassembly(instruction_t instruction) {
    std::stringstream ss{};
    switch (instruction.first_nibble()) {
    case F:
      if (instruction.third_nibble() == 1 && instruction.fourth_nibble() == E) {
        // FX1E add V[X] to I
        {
          ss << instruction;
          return {"FX1E", ss.str(), "Add to index"};
        };
        break;
      }
      if (instruction.third_nibble() == 2 && instruction.fourth_nibble() == 9) {
        // FX29 weird font instruction
        {
          ss << instruction;
          return {"FX29", ss.str(), "Font character"};
        };
        break;
      }
      if (instruction.third_nibble() == 3 && instruction.fourth_nibble() == 3) {
        // FX33 bcd representation
        {
          ss << instruction;
          return {"FX33", ss.str(), "Binary-coded decimal conversion"};
        };
        break;
      }
      if (instruction.third_nibble() == 5 && instruction.fourth_nibble() == 5) {
        // FX55 weird store
        {
          ss << instruction;
          return {"FX55", ss.str(), "Store memory"};
        };
        break;
      }
      if (instruction.third_nibble() == 6 && instruction.fourth_nibble() == 5) {
        // FX65 weird store
        {
          ss << instruction;
          return {"FX65", ss.str(), "Load memory"};
        };
        break;
      }
      if (instruction.third_nibble() == 0 && instruction.fourth_nibble() == A) {
        // FX0A Get Key
        {
          ss << instruction;
          return {"FX0A", ss.str(), "Get key"};
        };
        break;
      }
      if (instruction.third_nibble() == 0 && instruction.fourth_nibble() == 7) {
        // FX07
        {
          ss << instruction;
          return {"FX07", ss.str(), "Set VX = delay"};
        };
        break;
      }
      if (instruction.third_nibble() == 1 && instruction.fourth_nibble() == 5) {
        // FX15
        {
          ss << instruction;
          return {"FX15", ss.str(), "Set delay = VX"};
        };
        break;
      }
      if (instruction.third_nibble() == 1 && instruction.fourth_nibble() == 8) {
        // FX18
        {
          ss << instruction;
          return {"FX18", ss.str(), "Set sound = VX"};
        };
        break;
      }
      ss << instruction;
      return {"XXXX", ss.str(), ""};
      break;
    case E:
      if (instruction.third_nibble() == 9 && instruction.fourth_nibble() == E) {
        // EX9E skip if key
        {
          ss << instruction;
          return {"EX9E", ss.str(), "Skip if Key"};
        };
        break;
      }
      if (instruction.third_nibble() == A && instruction.fourth_nibble() == 1) {
        // EXA1 skip if not key
        {
          ss << instruction;
          return {"EXA1", ss.str(), "Skip if not key"};
        };
        break;
      }
      ss << instruction;
      return {"XXXX", ss.str(), ""};
      break;
    case D:
      // DXYN
      {
        ss << instruction;
        return {"DXYN", ss.str(), "Draw"};
      };
      break;
      break;
    case C:
      // CXNN random
      {
        ss << instruction;
        return {"CXNN", ss.str(), "Random"};
      };
      break;
      break;
    case B:
      // BNNN
      {
        ss << instruction;
        return {"BNNN", ss.str(), "Jump with offset"};
      };
      break;
      break;
    case A:
      // ANNN
      {
        ss << instruction;
        return {"ANNN", ss.str(), "Set index"};
      };
      break;
      break;
    case 9:
      // 9XY0 skip next instruction if the 2 selected registers are not equal
      {
        ss << instruction;
        return {"9XY0", ss.str(), "Skip if VX != VY"};
      };
      break;
      break;
    case 8:
      if (instruction.fourth_nibble() == 0) {
        // 8XY0 set V[X] to V[Y], V[Y] isnt affected
        {
          ss << instruction;
          return {"8XY0", ss.str(), "Set VX = VY"};
        };
        break;
      } else if (instruction.fourth_nibble() == 1) {
        // 8XY1 or
        {
          ss << instruction;
          return {"8XY1", ss.str(), "Or"};
        };
        break;
      } else if (instruction.fourth_nibble() == 2) {
        // 8XY2 and
        {
          ss << instruction;
          return {"8XY2", ss.str(), "And"};
        };
        break;
      } else if (instruction.fourth_nibble() == 3) {
        // 8XY3 xor
        {
          ss << instruction;
          return {"8XY3", ss.str(), "Xor"};
        };
        break;
      } else if (instruction.fourth_nibble() == 4) {
        // 8XY4 add
        {
          ss << instruction;
          return {"8XY4", ss.str(), "Add and set flag"};
        };
        break;
      } else if (instruction.fourth_nibble() == 5) {
        // 8XY5 sub V[X] = V[X] - V[Y]
        {
          ss << instruction;
          return {"8XY5", ss.str(), "Sub"};
        };
        break;
      } else if (instruction.fourth_nibble() == 6) {
        // 8XY6 shift V[X] right
        {
          ss << instruction;
          return {"8XY6", ss.str(), "Shift right"};
        };
        break;
      } else if (instruction.fourth_nibble() == 7) {
        // 8XY7 sub V[X] = V[Y] - V[X]
        {
          ss << instruction;
          return {"8XY7", ss.str(), "Sub reversed"};
        };
        break;
      } else if (instruction.fourth_nibble() == E) {
        // 8XYE shift V[X] left
        {
          ss << instruction;
          return {"8XYE", ss.str(), "Shift Left"};
        };
        break;
      }
      break;
    case 7:
      // 7XNN add NN to selected register
      {
        ss << instruction;
        return {"7XNN", ss.str(), "Add without setting flag"};
      };
      break;
      break;
    case 6:
      // 6XNN set NN to selected register
      {
        ss << instruction;
        return {"6XNN", ss.str(), "Set VX = NN"};
      };
      break;
      break;
    case 5:
      // 5XY0 skip next instruction if the 2 selected registers are equal
      {
        ss << instruction;
        return {"5XY0", ss.str(), "Skip if VX == VY"};
      };
      break;
      break;
    case 4:
      // 4XNN skip next instruction if number does not equals selected register
      {
        ss << instruction;
        return {"4XNN", ss.str(), "Skip if VX != NN"};
      };
      break;
      break;
    case 3:
      // 3XNN skip next instruction if number equals selected register
      {
        ss << instruction;
        return {"3XNN", ss.str(), "Skip if VX == NN"};
      };
      break;
      break;
    case 2:
      // 2NNN call
      {
        ss << instruction;
        return {"2NNN", ss.str(), "Call"};
      };
      break;
      break;
    case 1:
      // 1NNN jump
      {
        ss << instruction;
        return {"1NNN", ss.str(), "Jump"};
      };
      break;
      break;
    case 0:
      if (instruction.second_nibble() != 0) {
        ss << instruction;
        return {"XXXX", ss.str(), ""};
      }
      if (instruction.third_nibble() != E) {
        ss << instruction;
        return {"XXXX", ss.str(), ""};
      }
      if (instruction.fourth_nibble() == 0) {
        // 00E0 clear_screen
        {
          ss << instruction;
          return {"00E0", ss.str(), "Clear Screen"};
        };
      } else if (instruction.fourth_nibble() == E)
      // 00EE ret
      {
        ss << instruction;
        return {"00EE", ss.str(), "Return"};
      };
      ss << instruction;
      return {"XXXX", ss.str(), ""};
      break;
    default:
      ss << instruction;
      return {"XXXX", ss.str(), ""};
    }

    return {"XXXX", ss.str(), ""};
  }

  void decode_and_execute() {
    switch (instruction.first_nibble()) {
    case F:
      if (instruction.third_nibble() == 1 && instruction.fourth_nibble() == E) {
        // FX1E add V[X] to I
        {
          I += V[instruction.second_nibble()];
        };
        break;
      }
      if (instruction.third_nibble() == 2 && instruction.fourth_nibble() == 9) {
        // FX29 weird font instruction
        {
          uint16_t X = instruction.second_nibble();
          I = font_address + V[X] * 5;
        };
        break;
      }
      if (instruction.third_nibble() == 3 && instruction.fourth_nibble() == 3) {
        // FX33 bcd representation
        {
          uint16_t X = instruction.second_nibble();
          uint16_t value = V[X];
          memory[I + 2] = value % 10;
          value /= 10;
          memory[I + 1] = value % 10;
          value /= 10;
          memory[I + 0] = value % 10;
        };
        break;
      }
      if (instruction.third_nibble() == 5 && instruction.fourth_nibble() == 5) {
        // FX55 weird store
        {
          for (uint32_t i = 0; i <= instruction.second_nibble(); i++) {
            memory[I + i] = V[i];
          }
          I += instruction.second_nibble() + 1;
        };
        break;
      }
      if (instruction.third_nibble() == 6 && instruction.fourth_nibble() == 5) {
        // FX65 weird store
        {
          for (uint32_t i = 0; i <= instruction.second_nibble(); i++) {
            V[i] = memory[I + i];
          }
          I += instruction.second_nibble() + 1;
        };
        break;
      }
      if (instruction.third_nibble() == 0 && instruction.fourth_nibble() == A) {
        // FX0A Get Key
        {
          bool get_key = false;
          uint16_t X = instruction.second_nibble();
          static bool got = false;
          for (uint32_t i = 0; i < 16; i++) {
            if (keys[i]) {
              V[X] = i;
              get_key = true;
              got = true;
              break;
            }
          }
          if (!got)
            pc -= 2;
          if (got) {
            if (keys[V[X]])
              pc -= 2;
            else
              got = false;
          } else {
          }
        };
        break;
      }
      if (instruction.third_nibble() == 0 && instruction.fourth_nibble() == 7) {
        // FX07
        {
          uint16_t X = instruction.second_nibble();
          V[X] = delay;
        };
        break;
      }
      if (instruction.third_nibble() == 1 && instruction.fourth_nibble() == 5) {
        // FX15
        {
          uint16_t X = instruction.second_nibble();
          delay = V[X];
        };
        break;
      }
      if (instruction.third_nibble() == 1 && instruction.fourth_nibble() == 8) {
        // FX18
        {
          uint16_t X = instruction.second_nibble();
          sound = V[X];
        };
        break;
      }
      throw_unknown_instruction(instruction);
      break;
    case E:
      if (instruction.third_nibble() == 9 && instruction.fourth_nibble() == E) {
        // EX9E skip if key
        {
          uint16_t X = instruction.second_nibble();
          uint8_t key_id = V[X] & F;
          if (keys[key_id]) {
            pc += 2;
          }
        }
        break;
      }
      if (instruction.third_nibble() == A && instruction.fourth_nibble() == 1) {
        // EXA1 skip if not key
        {
          uint16_t X = instruction.second_nibble();
          uint8_t key_id = V[X] & F;
          if (!keys[key_id]) {
            pc += 2;
          }
        }
        break;
      }
      throw_unknown_instruction(instruction);
      break;
    case D:
      // DXYN
      {
        uint16_t X = instruction.second_nibble();
        uint16_t Y = instruction.third_nibble();
        uint16_t x = V[X] % width;
        uint16_t y = V[Y] % height;
        VF = 0;
        for (uint32_t row = 0; row < instruction.fourth_nibble(); row++) {
          uint8_t sprite_byte = memory[I + row];
          for (uint32_t col = 0; col < 8; col++) {
            uint8_t sprite_pixel = sprite_byte & (0x80u >> col);
            if (sprite_pixel) {
              if (y + row >= height || x + col >= width)
                continue;
              auto &p = fb[(y + row) * width + (x + col) % width];
              if (p == on_color) {
                VF = 1;
                p = off_color;
              } else {
                p = on_color;
              }
            }
          }
        }
      };
      break;
      break;
    case C:
      // CXNN random
      {
        int r = std::rand() % 0xFF; // random between 0 and 0xFF
        uint16_t X = instruction.second_nibble();
        V[X] = r & instruction.lower_byte();
      };
      break;
      break;
    case B:
      // BNNN
      {
        pc = instruction.address() + V[0];
      };
      break;
      break;
    case A:
      // ANNN
      {
        I = instruction.address();
      };
      break;
      break;
    case 9:
      // 9XY0 skip next instruction if the 2 selected registers are not equal
      {
        if (instruction.fourth_nibble() == 0) {
          uint16_t X = instruction.second_nibble();
          uint16_t Y = instruction.third_nibble();
          if (V[X] != V[Y]) {
            pc += 2;
          }
        }
      };
      break;
      break;
    case 8:
      if (instruction.fourth_nibble() == 0) {
        // 8XY0 set V[X] to V[Y], V[Y] isnt affected
        {
          uint16_t X = instruction.second_nibble();
          uint16_t Y = instruction.third_nibble();
          V[X] = V[Y];
        };
        break;
      } else if (instruction.fourth_nibble() == 1) {
        // 8XY1 or
        {
          uint16_t X = instruction.second_nibble();
          uint16_t Y = instruction.third_nibble();
          V[X] = V[X] | V[Y];
          VF = 0;
        };
        break;
      } else if (instruction.fourth_nibble() == 2) {
        // 8XY2 and
        {
          uint16_t X = instruction.second_nibble();
          uint16_t Y = instruction.third_nibble();
          V[X] = V[X] & V[Y];
          VF = 0;
        };
        break;
      } else if (instruction.fourth_nibble() == 3) {
        // 8XY3 xor
        {
          uint16_t X = instruction.second_nibble();
          uint16_t Y = instruction.third_nibble();
          V[X] = V[X] ^ V[Y];
          VF = 0;
        };
        break;
      } else if (instruction.fourth_nibble() == 4) {
        // 8XY4 add
        {
          uint16_t X = instruction.second_nibble();
          uint16_t Y = instruction.third_nibble();
          uint16_t result = V[X] + V[Y];
          V[X] = 0x00FF & result;
          if (result >= 0xFF) {
            VF = 1;
          } else {
            VF = 0;
          }
        };
        break;
      } else if (instruction.fourth_nibble() == 5) {
        // 8XY5 sub V[X] = V[X] - V[Y]
        {
          uint16_t X = instruction.second_nibble();
          uint16_t Y = instruction.third_nibble();
          uint8_t first = V[X];
          uint8_t second = V[Y];
          uint16_t result = V[X] - V[Y];
          V[X] = 0x00FF & result;
          if (second > first) {
            V[F] = 0;
          } else {
            V[F] = 1;
          }
        };
        break;
      } else if (instruction.fourth_nibble() == 6) {
        // 8XY6 shift V[X] right
        {
          uint16_t X = instruction.second_nibble();
          uint16_t Y = instruction.third_nibble();
          uint8_t result = V[X] >> 1;
          uint8_t flag = V[X] & 0x01;
          V[X] = result;
          V[F] = flag;
        };
        break;
      } else if (instruction.fourth_nibble() == 7) {
        // 8XY7 sub V[X] = V[Y] - V[X]
        {
          uint16_t X = instruction.second_nibble();
          uint16_t Y = instruction.third_nibble();
          uint8_t first = V[Y];
          uint8_t second = V[X];
          uint16_t result = V[Y] - V[X];
          V[X] = 0x00FF & result;
          if (second > first) {
            V[F] = 0;
          } else {
            V[F] = 1;
          }
        };
        break;
      } else if (instruction.fourth_nibble() == E) {
        // 8XYE shift V[X] left
        {

          uint16_t X = instruction.second_nibble();
          uint16_t Y = instruction.third_nibble();
          uint8_t result = V[X] << 1;
          uint8_t flag = (V[X] & 0x80) >> 7;
          V[X] = result;
          V[F] = flag;
        };
        break;
      }
      break;
    case 7:
      // 7XNN add NN to selected register
      {
        uint16_t X = instruction.second_nibble();
        V[X] += instruction.lower_byte();
      };
      break;
      break;
    case 6:
      // 6XNN set NN to selected register
      {
        uint16_t X = instruction.second_nibble();
        V[X] = instruction.lower_byte();
      };
      break;
      break;
    case 5:
      // 5XY0 skip next instruction if the 2 selected registers are equal
      {
        if (instruction.fourth_nibble() == 0) {
          uint16_t X = instruction.second_nibble();
          uint16_t Y = instruction.third_nibble();
          if (V[X] == V[Y]) {
            pc += 2;
          }
        }
      };
      break;
      break;
    case 4:
      // 4XNN skip next instruction if number does not equals selected register
      {
        uint16_t X = instruction.second_nibble();
        if (V[X] != instruction.lower_byte()) {
          pc += 2;
        }
      };
      break;
      break;
    case 3:
      // 3XNN skip next instruction if number equals selected register
      {
        uint16_t X = instruction.second_nibble();
        if (V[X] == instruction.lower_byte()) {
          pc += 2;
        }
      };
      break;
      break;
    case 2:
      // 2NNN call
      {
        stack.push(pc);
        pc = instruction.address();
      };
      break;
      break;
    case 1:
      // 1NNN jump
      {
        pc = instruction.address();
      };
      break;
      break;
    case 0:
      if (instruction.second_nibble() != 0)
        throw_unknown_instruction(instruction);
      if (instruction.third_nibble() != E)
        throw_unknown_instruction(instruction);
      if (instruction.fourth_nibble() == 0) {
        // 00E0 clear_screen
        {
          for (auto &pixel : fb)
            pixel = off_color;
        };
      } else if (instruction.fourth_nibble() == E)
      // 00EE ret
      {
        pc = stack.top();
        stack.pop();
      } else
        throw_unknown_instruction(instruction);
      break;
    default:
      throw_unknown_instruction(instruction);
    }
  }

  void execute(std::function<void(void)> fn) { fn(); }
};

template <size_t width, size_t height>
std::ostream &operator<<(std::ostream &o, const chip8_t<width, height> &chip8) {
  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      if (chip8.fb[y * width + x]) {
        o << "\u25A0";
      } else {
        o << ' ';
      }
    }
    o << '\n';
  }
  o << std::hex << "pc: " << chip8.pc << " index: " << chip8.I << '\n';
  for (uint32_t i = 0; i < 16; i++) {
    o << "V[" << i << "]: " << std::setw(2) << std::hex << (uint16_t)chip8.V[i]
      << ' ';
    if ((i + 1) % 4 == 0)
      o << '\n';
  }
  o << std::dec;
  return o;
}

#endif
