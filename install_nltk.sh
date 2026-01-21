#!/bin/bash
# Installation facile de NLTK

echo "📦 Installation de NLTK pour SwiftFlow Pro..."
echo "========================================"

# Vérifier Python
if ! command -v python3 >/dev/null 2>&1; then
    echo "❌ Python3 n'est pas installé"
    echo "Installation: sudo apt-get install python3 python3-pip"
    exit 1
fi

# Installer NLTK
echo "1. Installation de NLTK..."
pip3 install nltk colorama --quiet 2>/dev/null || pip install nltk colorama --quiet 2>/dev/null || {
    echo "❌ Échec installation pip, tentative avec apt..."
    sudo apt-get install python3-nltk python3-colorama -y 2>/dev/null || {
        echo "❌ Impossible d'installer NLTK"
        echo "Le REPL fonctionnera sans coloration avancée"
    }
}

# Télécharger les données
echo "2. Téléchargement des données NLTK..."
python3 << 'PYTHON'
import nltk
import sys

print("Téléchargement en cours... (cela peut prendre un moment)")

try:
    # Créer le dossier nltk_data
    nltk.data.path.append('/usr/local/share/nltk_data')
    
    # Télécharger les données nécessaires
    nltk.download('punkt', quiet=True)
    nltk.download('punkt_tab', quiet=True)
    
    print("✅ Données NLTK téléchargées!")
    print("Emplacement: ~/nltk_data/")
    
    # Tester
    from nltk import word_tokenize
    test = word_tokenize("Hello SwiftFlow")
    print(f"Test réussi: {test}")
    
except Exception as e:
    print(f"⚠️  Erreur: {e}")
    print("Le REPL simple sera utilisé à la place")
PYTHON

echo ""
echo "✅ Installation terminée!"
echo ""
echo "Pour tester:"
echo "  python3 -c \"import nltk; print('NLTK importé avec succès!')\""
echo ""
echo "Le REPL SwiftFlow utilisera automatiquement:"
echo "  • REPL avancé si NLTK est disponible"
echo "  • REPL simple sinon (pas de problème)"
