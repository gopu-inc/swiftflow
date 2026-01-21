#!/bin/bash

# Script de setup pour les tests SwiftFlow

echo "🔧 Configuration des tests SwiftFlow..."

# Créer les répertoires nécessaires
sudo mkdir -p /usr/local/lib/swift/stdlib
sudo mkdir -p /usr/local/lib/swift/modules

# Créer le fichier .svlib pour stdlib
sudo tee /usr/local/lib/swift/stdlib/stdlib.svlib > /dev/null << 'EOF'
// Fichier d'export pour stdlib
export "test_stdlib.swf" as "test_stdlib";
export "test_stdlib.swf" as "math_module";
EOF

# Créer les modules de test
echo "📦 Création des modules de test..."

# Module test_stdlib
sudo tee /usr/local/lib/swift/stdlib/test_stdlib.swf > /dev/null << 'EOF'
// Module de test pour la bibliothèque standard
// test_stdlib.swf

// Variables globales
var global_counter = 0;

// Fonction simple
func greet(name) {
    print("Hello, " + name + "!");
    return "Greeted " + name;
}

// Fonction avec paramètres multiples
func add(a, b) {
    return a + b;
}

// Fonction avec logique conditionnelle
func max(a, b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

// Fonction qui modifie une variable globale
func increment_counter() {
    global_counter = global_counter + 1;
    return global_counter;
}

// Fonction récursive (factorielle)
func factorial(n) {
    if (n <= 1) {
        return 1;
    } else {
        return n * factorial(n - 1);
    }
}

// Test des différents types
func test_types() {
    var int_val = 42;
    var float_val = 3.14;
    var str_val = "test";
    var bool_val = true;
    
    print("int: " + int_val);
    print("float: " + float_val);
    print("string: " + str_val);
    print("bool: " + bool_val);
    
    return "Types test completed";
}

// Exporter quelques fonctions
export "greet" as "greet";
export "add" as "add";
export "max" as "maximum";
export "test_types" as "type_test";

print("✅ Standard library module loaded successfully!");
print("   Available functions: greet(), add(), max(), increment_counter(), factorial(), test_types()");
EOF

# Module math_module
sudo tee /usr/local/lib/swift/modules/math_module.swf > /dev/null << 'EOF'
// Module mathématique
// math_module.swf

// Constantes mathématiques
const PI = 3.141592653589793;
const E = 2.718281828459045;

// Fonctions mathématiques
func square(x) {
    return x * x;
}

func cube(x) {
    return x * x * x;
}

func power(base, exponent) {
    var result = 1;
    var i = 0;
    
    while (i < exponent) {
        result = result * base;
        i = i + 1;
    }
    
    return result;
}

func is_even(n) {
    return (n % 2) == 0;
}

func is_prime(n) {
    if (n <= 1) {
        return false;
    }
    
    var i = 2;
    while (i * i <= n) {
        if (n % i == 0) {
            return false;
        }
        i = i + 1;
    }
    
    return true;
}

// Suite de Fibonacci
func fibonacci(n) {
    if (n <= 1) {
        return n;
    }
    
    var a = 0;
    var b = 1;
    var i = 2;
    
    while (i <= n) {
        var temp = a + b;
        a = b;
        b = temp;
        i = i + 1;
    }
    
    return b;
}

print("✅ Math module loaded!");
print("   PI = " + PI);
print("   E = " + E);
EOF

# Module local
tee local_module.swf > /dev/null << 'EOF'
// Module local pour tests
// local_module.swf

var local_var = "I'm a local variable";

func local_greet() {
    return "Hello from local module!";
}

func multiply(x, y) {
    return x * y;
}

func print_table(n) {
    var i = 1;
    while (i <= 10) {
        print(n + " x " + i + " = " + (n * i));
        i = i + 1;
    }
}

print("✅ Local module loaded!");
EOF

# Fichier principal de test
tee main_test.swf > /dev/null << 'EOF'
// Test complet de SwiftFlow
// main_test.swf

// ============================================
// TEST 1: Importations
// ============================================
print("\n=== TEST 1: Importations ===");

// Import depuis package stdlib
import "test_stdlib" from "stdlib";

// Import multiple
import "test_stdlib, math_module" from "stdlib";

// Import local
import "./local_module";

// ============================================
// TEST 2: Variables
// ============================================
print("\n=== TEST 2: Variables ===");

// Variables de différents types
var int_var = 42;
var float_var = 3.14159;
var string_var = "Hello World";
var bool_var = true;

// Variables spéciales
net network_var = 100;
clog clog_var = 200;
dos dos_var = 300;
sel sel_var = 400;

// Constante
const MAX_VALUE = 1000;

// Assignation
int_var = 99;
string_var = "Updated string";

// ============================================
// TEST 3: Expressions
// ============================================
print("\n=== TEST 3: Expressions ===");

// Opérations arithmétiques
var sum = 10 + 5;
var product = 10 * 5;
var division = 10 / 3;
var power = 2 ** 3; // 2^3 = 8

// Concaténation de strings
var greeting = "Hello" + " " + "World";

// Opérations logiques
var logical_and = true && false;
var logical_or = true || false;

// Comparaisons
var equal = (10 == 10);
var not_equal = (10 != 5);
var greater = (10 > 5);
var less = (5 < 10);

// ============================================
// TEST 4: Structures de contrôle
// ============================================
print("\n=== TEST 4: Structures de contrôle ===");

// If-else
if (int_var > 50) {
    print("int_var est supérieur à 50");
} else {
    print("int_var est inférieur ou égal à 50");
}

// While loop
var counter = 0;
while (counter < 3) {
    print("Counter: " + counter);
    counter = counter + 1;
}

// For loop
for (var i = 0; i < 3; i = i + 1) {
    print("For loop i: " + i);
}

// ============================================
// TEST 5: Fonctions
// ============================================
print("\n=== TEST 5: Fonctions ===");

// Définition de fonction
func my_add(x, y) {
    return x + y;
}

func print_message(msg) {
    print("Message: " + msg);
}

func factorial_recursive(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial_recursive(n - 1);
}

// Appels de fonction
var result = my_add(10, 20);
print("my_add(10, 20) = " + result);

print_message("Test message");

var fact_5 = factorial_recursive(5);
print("5! = " + fact_5);

// ============================================
// TEST 6: Appels de fonctions importées
// ============================================
print("\n=== TEST 6: Appels de fonctions importées ===");

// Depuis test_stdlib
greet("SwiftFlow User");

var sum_result = add(15, 25);
print("15 + 25 = " + sum_result);

var max_value = maximum(10, 20);
print("max(10, 20) = " + max_value);

// Depuis local_module
var local_result = multiply(7, 6);
print("7 * 6 = " + local_result);

// ============================================
// TEST 7: Opérations spéciales
// ============================================
print("\n=== TEST 7: Opérations spéciales ===");

// Taille de variable
print("Taille de int_var: ");
sizeof(int_var);

// Weld (input)
weld("Entrez votre nom: ");

// Pass (ne rien faire)
pass;

// ============================================
// TEST 8: Debug avec dbvar
// ============================================
print("\n=== TEST 8: Debug ===");

// Afficher toutes les variables
dbvar;

// ============================================
// TEST 9: Structures complexes
// ============================================
print("\n=== TEST 9: Structures complexes ===");

// Bloc avec variables locales
{
    var block_var = "Je suis dans un bloc";
    print(block_var);
    
    {
        var nested_var = "Je suis imbriqué";
        print(nested_var);
    }
}

// Fonction avec plusieurs instructions
func complex_function(x) {
    var result = x * 2;
    
    if (result > 10) {
        result = result - 5;
    } else {
        result = result + 5;
    }
    
    var i = 0;
    while (i < 3) {
        result = result + 1;
        i = i + 1;
    }
    
    return result;
}

var complex_result = complex_function(8);
print("complex_function(8) = " + complex_result);

// ============================================
// TEST 10: Gestion d'erreurs (basique)
// ============================================
print("\n=== TEST 10: Tests divers ===");

// Division par zéro
var safe_div = 10;
if (safe_div != 0) {
    var div_result = 100 / safe_div;
    print("100 / 10 = " + div_result);
}

// Variable non initialisée
var uninitialized;
print("Variable non initialisée: ");
print(uninitialized);

// Assignation à une constante (doit échouer)
const CONST_TEST = 100;
// CONST_TEST = 200; // Cette ligne devrait causer une erreur

// ============================================
// TEST 11: Retours multiples
// ============================================
print("\n=== TEST 11: Fin du programme ===");

// Retour final
return "Programme terminé avec succès!";

// ============================================
// Fonction main (point d'entrée)
// ============================================
main() {
    print("🚀 Démarrage des tests SwiftFlow...");
    
    // Tous les tests ci-dessus sont exécutés ici
    
    print("\n✅ Tous les tests sont terminés!");
    print("📊 Résumé:");
    print("   - Variables: ✓");
    print("   - Structures de contrôle: ✓");
    print("   - Fonctions: ✓");
    print("   - Importations: ✓");
    print("   - Modules: ✓");
    
    return "Tests réussis!";
}
EOF

# Fichier de test simple
tee simple_test.swf > /dev/null << 'EOF'
// Test simple
print("=== Test Simple ===");

var x = 10;
var y = 20;
var z = x + y;

print("x = " + x);
print("y = " + y);
print("x + y = " + z);

if (z > 25) {
    print("z est plus grand que 25");
} else {
    print("z est plus petit ou égal à 25");
}

print("=== Fin du test ===");
EOF

echo "✅ Setup terminé!"
echo ""
echo "📋 Fichiers créés:"
echo "   - /usr/local/lib/swift/stdlib/test_stdlib.swf"
echo "   - /usr/local/lib/swift/modules/math_module.swf"
echo "   - ./local_module.swf"
echo "   - ./main_test.swf (test complet)"
echo "   - ./simple_test.swf (test simple)"
echo ""
echo "🚀 Pour exécuter les tests:"
echo "   ./swift simple_test.swf    # Test simple"
echo "   ./swift main_test.swf      # Test complet"
echo "   ./swift                     # Mode REPL"