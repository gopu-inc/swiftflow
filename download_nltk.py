#!/usr/bin/env python3
import nltk
import sys

print("Téléchargement des données NLTK...")

try:
    # Télécharger les données nécessaires
    nltk.download('punkt', quiet=False)
    nltk.download('punkt_tab', quiet=False)
    nltk.download('averaged_perceptron_tagger', quiet=True)
    
    print("✅ Données NLTK téléchargées avec succès!")
    print("Emplacement: ~/nltk_data/")
    
except Exception as e:
    print(f"❌ Erreur: {e}")
    print("Le REPL fonctionnera sans coloration avancée")
