#!/bin/sh
echo "=== SwiftVelox Compiler ==="
echo "Compilation du compilateur..."
make

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Compilateur créé: ./swiftvelox"
    echo ""
    echo "Usage:"
    echo "  ./swiftvelox build examples/hello.svx"
    echo "  ./swiftvelox run examples/hello.svx"
    echo "  ./swiftvelox version"
else
    echo "❌ Erreur de compilation"
    exit 1
fi
