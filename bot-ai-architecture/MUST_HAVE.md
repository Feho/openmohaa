# Feuille de Route Essentielle pour l'Architecture IA (Must-Haves)

Ce document résume le chemin critique pour moderniser l'architecture IA de OpenMoHAA. Il est basé sur le plan complet mais se concentre sur les tâches qui offrent le maximum de valeur avec un effort optimisé, en reportant les fonctionnalités plus complexes ou moins prioritaires.

## Fonctionnalités Reportées ou Optionnelles (Deferred / Optional)

Pour économiser les ressources, les fonctionnalités suivantes sont considérées comme non essentielles pour la première version majeure de la nouvelle IA :

1.  **Refonte complète vers l'ECS (Task 4.5) :** Très coûteuse, à n'envisager que si des problèmes de performance extrêmes apparaissent et ne peuvent être résolus autrement.
2.  **Système de Plugins (Task 4.4) :** Un "luxe" pour la modularité à long terme, non requis pour l'IA de base.
3.  **Planificateur GOAP :** L'association Behavior Trees + Utility AI est déjà suffisamment puissante et moins complexe à mettre en œuvre.
4.  **Outils de Débogage Avancés (Enregistrement/Relecture) :** Les visualiseurs temps réel sont indispensables, mais le système d'enregistrement/relecture peut être reporté.

---

## Chemin Critique Recommandé (Recommended Critical Path)

Voici la séquence de tâches "allégée" recommandée pour une livraison efficace.

### Phase 1 : Fondation (À réaliser en entier)

*   **Objectif :** Améliorer la qualité du code existant, mettre en place les tests et préparer le terrain.
*   **Tâches :** Toutes les tâches de `tasks/phase-1-foundation.md` sont critiques.

### Phase 2 : Systèmes Cœur (À réaliser en entier)

*   **Objectif :** Mettre en place le squelette de la nouvelle architecture (Perception, Behavior Trees, Profils YAML) en parallèle de l'ancien système.
*   **Tâches :** Toutes les tâches de `tasks/phase-2-core-systems.md` sont critiques.

### Phase 3 : Migration et IA Avancée (Focus sur l'essentiel)

*   **Objectif :** Migrer les comportements existants vers le nouveau système et implémenter la logique de décision dynamique.
*   **Tâches à prioriser :**
    *   **Tasks 3.1, 3.2, 3.3 :** Migration de tous les comportements (Attaque, Investigation, Idle) vers les Behavior Trees.
    *   **Task 3.4 :** Implémentation de l'Utility AI pour le choix de la stratégie.
    *   **Partie de la Task 3.5 :** Création des **visualiseurs de débogage en temps réel** (Arbre de comportement, scores d'utilité, perception).

### Phase 4 : Polissage et Optimisation (Focus sur la performance)

*   **Objectif :** Assurer que le nouveau système est performant pour un grand nombre de bots.
*   **Tâches à prioriser :**
    *   **Task 4.1 :** Implémentation du système de **LOD (Level-of-Detail)** pour l'IA. C'est le gain de performance le plus important pour supporter de nombreux bots.
    *   **(Optionnel) Task 4.2 :** L'indexation spatiale si les requêtes de proximité s'avèrent être un goulot d'étranglement.

---

En suivant cette feuille de route allégée, le projet peut livrer 90% de la valeur de la nouvelle architecture avec environ 50% de l'effort total estimé, tout en conservant une base saine pour d'éventuelles améliorations futures.
