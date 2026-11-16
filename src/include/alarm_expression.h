/*
 * Archivo: alarm_expression.h
 * Descripción: Analiza y evalúa expresiones de alarma.
 * Necesita: Expresión infija o postfix con operandos y operadores válidos.
 * Retornos: Expresión transformada o resultado lógico; rechaza sintaxis inválida mediante
 * excepción.
 */

#ifndef DRIVER_ALARM_EXPRESSION_H
#define DRIVER_ALARM_EXPRESSION_H
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <limits>
#include <cctype>
inline int alarm_precedence(char op) {
    switch (op) {
    case '@':
        return 5;
    case '>':
    case '<':
    case '=':
    case '~':
        return 4;
    case '!':
        return 3;
    case '&':
        return 2;
    case '|':
        return 1;
    default:
        return 0;
    }
}
inline std::string alarm_postfix(const std::string &expression) {
    if (expression.size() > 4096)
        throw std::invalid_argument("alarm expression too long");
    std::vector<char> operators;
    std::string output;
    bool operand = true;
    for (size_t i = 0; i < expression.size();) {
        char c = expression[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' ||
            ((c == '-' || c == '+') && operand)) {
            if (!operand)
                throw std::invalid_argument("missing alarm operator");
            char *end;
            const char *begin = expression.c_str() + i;
            double number = std::strtod(begin, &end);
            if (end == begin || !std::isfinite(number))
                throw std::invalid_argument("invalid alarm number");
            output.append(begin, end - begin);
            output += ' ';
            i += end - begin;
            operand = false;
            continue;
        }
        ++i;
        if (c == '(') {
            if (!operand)
                throw std::invalid_argument("missing operator before parenthesis");
            operators.push_back(c);
            continue;
        }
        if (c == ')') {
            if (operand)
                throw std::invalid_argument("empty alarm operand");
            while (!operators.empty() && operators.back() != '(') {
                output += operators.back();
                output += ' ';
                operators.pop_back();
            }
            if (operators.empty())
                throw std::invalid_argument("unmatched closing parenthesis");
            operators.pop_back();
            operand = false;
            continue;
        }
        if (!alarm_precedence(c) || (operand && c != '!') || (!operand && c == '!'))
            throw std::invalid_argument("invalid alarm operator");
        while (!operators.empty() && operators.back() != '(' &&
               (alarm_precedence(c) < alarm_precedence(operators.back()) ||
                (alarm_precedence(c) == alarm_precedence(operators.back()) && c != '!'))) {
            output += operators.back();
            output += ' ';
            operators.pop_back();
        }
        operators.push_back(c);
        operand = true;
    }
    if (operand)
        throw std::invalid_argument("incomplete alarm expression");
    while (!operators.empty()) {
        if (operators.back() == '(')
            throw std::invalid_argument("unmatched opening parenthesis");
        output += operators.back();
        output += ' ';
        operators.pop_back();
    }
    return output;
}
inline int alarm_evaluate_postfix(const std::string &expression) {
    std::istringstream tokens(expression);
    std::vector<double> values;
    std::string token;
    while (tokens >> token) {
        const char op = token[0];
        if (token.size() == 1 && alarm_precedence(op)) {
            const size_t needed = op == '!' ? 1 : 2;
            if (values.size() < needed)
                throw std::invalid_argument("not enough alarm operands");
            double right = values.back();
            values.pop_back();
            if (op == '!') {
                values.push_back(!right);
                continue;
            }
            double left = values.back();
            values.pop_back();
            switch (op) {
            case '&':
                values.push_back(left && right);
                break;
            case '|':
                values.push_back(left || right);
                break;
            case '>':
                values.push_back(left > right);
                break;
            case '<':
                values.push_back(left < right);
                break;
            case '=':
                values.push_back(left == right);
                break;
            case '~':
                values.push_back(left != right);
                break;
            case '@': {
                for (double value : {left, right})
                    if (static_cast<long double>(value) < std::numeric_limits<int64_t>::min() ||
                        static_cast<long double>(value) > std::numeric_limits<int64_t>::max())
                        throw std::invalid_argument("bitwise operand out of range");
                values.push_back(static_cast<int64_t>(left) & static_cast<int64_t>(right));
                break;
            }
            }
        } else {
            char *end;
            double value = std::strtod(token.c_str(), &end);
            if (*end || end == token.c_str() || !std::isfinite(value))
                throw std::invalid_argument("invalid postfix number");
            values.push_back(value);
        }
    }
    if (values.size() != 1)
        throw std::invalid_argument("invalid alarm operand count");
    return values.back() != 0;
}
#endif
