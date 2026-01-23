#!/bin/bash
# fix-parser.sh

echo "🔧 Correction de parser.c..."

# Trouver la ligne où commencent les déclarations static
LINE=$(grep -n "static ASTNode\* parse_try_statement" src/parser.c | head -1 | cut -d: -f1)

if [ -n "$LINE" ]; then
    # Insérer parse_throw_statement avant parse_try_statement
    sed -i "${LINE}i static ASTNode* parse_throw_statement(Parser* parser);" src/parser.c
    echo "✅ parse_throw_statement ajouté aux déclarations forward"
else
    echo "⚠️  Impossible de trouver les déclarations, correction manuelle nécessaire"
    echo "Ajoutez cette ligne au début de parser.c:"
    echo "static ASTNode* parse_throw_statement(Parser* parser);"
fi

# Supprimer la variable error_var non utilisée dans parse_try_statement
sed -i '/Token error_var = parser_consume/s/^/\/\//' src/parser.c
echo "✅ Variable error_var commentée"

echo ""
echo "Recompilez avec: make"
