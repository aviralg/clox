#include <__format/format_functions.h>
#include <iostream>
#include <vector>
#include <string>
#include <ostream>
#include <format>
#include <cassert>

#define FAIL(msg) do { std::cerr << std::format("[FAIL]-[%s]-[%06d]- %s", __FILE__, __LINE__, msg); exit(1); } while(false)
#define FAIL_IF(cond, msg) do { if (cond) FAIL(msg) } while(false)

#define DEBUG_TRACE_EXECUTION

class Instruction {
public:
    enum class Opcode : uint8_t {
        Return,
        Constant,
        Add,
        Subtract,
        Multiply,
        Divide,
        Negate,
        _Count,
    };

    Instruction(const uint line, const Opcode opcode): line_(line), opcode_(opcode) {
        
    }

    Instruction(const uint line, const Opcode opcode, uint index): line_(line), opcode_(opcode), index_(index) {
        
    }

    uint get_line() const {
        return line_;
    }

    Opcode get_opcode() const {
        return opcode_;
    }

    size_t get_constant_index() const {
        return index_;
    }

    static Instruction deserialize(const uint line, const uint32_t bytes) {
        Opcode opcode = static_cast<Instruction::Opcode>(bytes & 0xff);
        switch (opcode) {
        case Opcode::Return:
        case Opcode::Add:
        case Opcode::Subtract:
        case Opcode::Multiply:
        case Opcode::Divide:
        case Opcode::Negate:
            return Instruction(line, opcode);
        case Opcode::Constant: {
            uint index = bytes >> 8;
            return Instruction(line, opcode, index);
        }
        case Opcode::_Count:
            FAIL("invalid opcode '_COUNT'");
        }
    }

    std::pair<uint, uint32_t> serialize() const {
        uint32_t bytecode = static_cast<uint8_t>(opcode_);
        switch (opcode_) {
        case Opcode::Return:
        case Opcode::Add:
        case Opcode::Subtract:
        case Opcode::Multiply:
        case Opcode::Divide:
        case Opcode::Negate:
            break;
        case Opcode::Constant:
            bytecode |= (index_ << 8);
            break;
        case Opcode::_Count:
            FAIL("invalid opcode '_COUNT'");
        }
        return {line_, bytecode};
    }

private:
    uint line_;
    Opcode opcode_;
    uint index_: 24;
};

std::string to_string(Instruction::Opcode opcode) {
    switch(opcode) {
    case Instruction::Opcode::Return: return "RETURN";
    case Instruction::Opcode::Constant: return "CONSTANT";
    case Instruction::Opcode::Add: return "ADD";
    case Instruction::Opcode::Subtract: return "SUBTRACT";
    case Instruction::Opcode::Multiply: return "MULTIPLY";
    case Instruction::Opcode::Divide: return "DIVIDE";
    case Instruction::Opcode::Negate: return "NEGATE";
    default:
        FAIL("invalid opcode");
    }
}

class ConstantPool {
public:
    ConstantPool() {
    }

    double get(const size_t offset) const {
        return constants_.at(offset);
    }

    size_t add(const double constant) {
        constants_.push_back(constant);
        return constants_.size() - 1;
    }

private:
    std::vector<double> constants_;
};

class Chunk {
public:
    Chunk(std::string_view name) : name_(name) {
    }

    const std::string& get_name() const {
        return name_;
    }

    size_t get_length() const {
        return codes_.size();
    }

    Instruction read(const size_t offset) const {
        return Instruction::deserialize(lines_.at(offset), codes_.at(offset));
    }

    void write(const Instruction& instruction) {
        auto [line, code] = instruction.serialize();
        lines_.push_back(line);
        codes_.push_back(code);
    }

    ConstantPool& get_constant_pool() {
        return constant_pool_;
    }

    const ConstantPool& get_constant_pool() const {
        return constant_pool_;
    }

    void disassemble(std::ostream& out) {
        out << std::format("== {} ==", get_name()) << std::endl;

        for(size_t index = 0; index < get_length(); ++index) {
            disassemble(out, index);
        }
    }

    void disassemble(std::ostream& out, const size_t index) {
        const uint32_t code = codes_.at(index);
        Instruction instr = read(index);
        const uint line = instr.get_line();
        const Instruction::Opcode opcode = instr.get_opcode();
        
        std::string index_str = std::format("{:0>6}", index);
        std::string line_str = std::format("{:0>5}", line);
        std::string code_str = std::format("{:0>32b}", code);
        std::string opcode_str = std::format("{:>10}", to_string(instr.get_opcode())); 

        out << index_str << "  " << line_str << "  " << code_str << "  " << opcode_str << "  ";

        switch (opcode) {
        case Instruction::Opcode::Return:
        case Instruction::Opcode::Add:
        case Instruction::Opcode::Subtract:
        case Instruction::Opcode::Multiply:
        case Instruction::Opcode::Divide:
        case Instruction::Opcode::Negate:
            break;
        case Instruction::Opcode::Constant: {
            size_t constant_index = instr.get_constant_index();
            double value = get_constant_pool().get(constant_index);
            out << std::format("{:>6}  {:>10}", index, value);
            break;
        }
        case Instruction::Opcode::_Count:
            FAIL("opcode cannot be 'OP__COUNT'");
            break;
        }
        
        out << std::endl;
    }
    
private:
    std::string name_;
    std::vector<uint> lines_;
    std::vector<uint32_t> codes_;
    ConstantPool constant_pool_;
};

class VirtualMachine {
public:
    enum class Result {
        Ok,
        CompileError,
        RuntimeError,
    };

    VirtualMachine(Chunk* chunk): chunk_(chunk), ip_(0) {
        
    }

    Result run(std::ostream& out) {

        while(ip_ < chunk_->get_length()) {

#ifdef DEBUG_TRACE_EXECUTION
            dump_(out);
#endif

            Instruction instr = chunk_->read(ip_);
            step(out, instr);
            ip_++;
        }
        
        return Result::Ok;
    }    
    
    void step(std::ostream& out, const Instruction instr) {
        
        auto opcode = instr.get_opcode();
        switch(opcode) {
        case Instruction::Opcode::Return: {
            double value = pop_();
            out << value << std::endl;
            break;
        }
        case Instruction::Opcode::Add: {
            double right = pop_();
            double left = pop_();
            double result = left + right;
            push_(result);
            break;
        }
        case Instruction::Opcode::Subtract: {
            double right = pop_();
            double left = pop_();
            double result = left - right;
            push_(result);
            break;
        }
        case Instruction::Opcode::Multiply: {
            double right = pop_();
            double left = pop_();
            double result = left * right;
            push_(result);
            break;
        }
        case Instruction::Opcode::Divide: {
            double right = pop_();
            double left = pop_();
            double result = left / right;
            push_(result);
            break;
        }
        case Instruction::Opcode::Negate: {
            push_(-pop_());
            break;
        }
        case Instruction::Opcode::Constant: {
            double value = chunk_->get_constant_pool().get(instr.get_constant_index());
            push_(value);
            break;
        }
        default:
            FAIL("invalid opcode");
            break;
        }
    }

private:
    Chunk* chunk_;
    size_t ip_;
    std::vector<double> stack_;

    void dump_(std::ostream& out) {
        out << "[ ";
        for(size_t i = 0; i < stack_.size(); ++i) {
            out << stack_[i] << " ";
        }
        out << "]" << std::endl;
        chunk_->disassemble(out, ip_);
    }
    
    void push_(const double value) {
        stack_.push_back(value);
    }

    double pop_() {
        double value = stack_.back();
        stack_.pop_back();
        return value;
    }
};

int main(int argc, char* argv[]) {
    Chunk chunk("test chunk");
    chunk.write(Instruction(1, Instruction::Opcode::Constant, chunk.get_constant_pool().add(1.2)));
    chunk.write(Instruction(2, Instruction::Opcode::Constant, chunk.get_constant_pool().add(3.4)));
    chunk.write(Instruction(3, Instruction::Opcode::Add));
    chunk.write(Instruction(4, Instruction::Opcode::Constant, chunk.get_constant_pool().add(5.6)));
    chunk.write(Instruction(5, Instruction::Opcode::Divide));
    chunk.write(Instruction(6, Instruction::Opcode::Negate));
    chunk.write(Instruction(7, Instruction::Opcode::Return));
    VirtualMachine vm(&chunk);
    vm.run(std::cerr);
    
    return 0;
}
