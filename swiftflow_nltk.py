#!/usr/bin/env python3
# Module NLTK pour coloration syntaxique SwiftFlow

import nltk
import sys
import re
from colorama import init, Fore, Back, Style

# Télécharger les données NLTK si nécessaire
try:
    nltk.data.find('tokenizers/punkt')
except LookupError:
    nltk.download('punkt', quiet=True)

init(autoreset=True)

# Catégories de tokens SwiftFlow
KEYWORDS = {
    'var', 'net', 'clog', 'dos', 'sel', 'let', 'const',
    'if', 'else', 'elif', 'while', 'for', 'do',
    'func', 'return', 'import', 'export', 'from',
    'print', 'weld', 'pass', 'dbvar', 'main',
    'true', 'false', 'null', 'undefined'
}

TYPES = {
    'int', 'float', 'string', 'bool', 'char', 'void',
    'any', 'auto'
}

OPERATORS = {
    '+', '-', '*', '/', '%', '**', '=', '==', '!=',
    '>', '<', '>=', '<=', 'and', 'or', 'not', 'in', 'is'
}

def colorize_swiftflow(code):
    """Colorise le code SwiftFlow avec NLTK"""
    if not code.strip():
        return code
    
    # Séparer en tokens avec NLTK
    tokens = nltk.word_tokenize(code)
    
    result = []
    in_string = False
    string_delimiter = ''
    
    for token in tokens:
        # Gérer les strings
        if token in ['"', "'"] and not in_string:
            in_string = True
            string_delimiter = token
            result.append(Fore.GREEN + token)
        elif token == string_delimiter and in_string:
            in_string = False
            result.append(Fore.GREEN + token)
            string_delimiter = ''
        elif in_string:
            result.append(Fore.GREEN + token)
        
        # Colorisation selon le type
        elif token in KEYWORDS:
            result.append(Fore.CYAN + Style.BRIGHT + token)
        elif token in TYPES:
            result.append(Fore.MAGENTA + token)
        elif token in OPERATORS:
            result.append(Fore.YELLOW + token)
        elif token.isdigit() or (token.replace('.', '', 1).isdigit() and token.count('.') < 2):
            result.append(Fore.BLUE + token)
        elif re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*$', token):
            result.append(Fore.WHITE + token)
        else:
            result.append(token)
    
    return ' '.join(result)

def suggest_completions(code):
    """Suggère des complétions basées sur le code"""
    suggestions = []
    
    if 'print' in code and '(' not in code:
        suggestions.append("print()")
    if 'if' in code and '(' not in code:
        suggestions.append("if () {")
    if 'while' in code and '(' not in code:
        suggestions.append("while () {")
    if 'func' in code:
        suggestions.append("func name() {")
    if 'var' in code and '=' not in code:
        suggestions.append("var name = value")
    
    # Suggestions basées sur les mots-clés
    last_word = code.strip().split()[-1] if code.strip().split() else ""
    
    if last_word == 'import':
        suggestions.append('"module" from "package"')
    elif last_word == 'print':
        suggestions.append('("message")')
    elif last_word == 'if':
        suggestions.append('(condition) {')
    
    return suggestions[:3]  # Retourne max 3 suggestions

if __name__ == "__main__":
    if len(sys.argv) > 1:
        # Mode ligne de commande
        code = ' '.join(sys.argv[1:])
        print(colorize_swiftflow(code))
    else:
        # Mode test
        test_code = 'var x = 10; print("Hello"); if (x > 5) { weld("Big"); }'
        print("Test de coloration:")
        print(colorize_swiftflow(test_code))
