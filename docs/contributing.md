# Guide de contribution

Merci de votre intérêt pour servd ! Voici comment contribuer efficacement.

## Processus

1. **Discutez** d'abord de votre idée via une issue GitHub
2. **Forkez** le dépôt et créez une branche
3. **Implémentez** en respectant les [conventions](conventions.md)
4. **Testez** manuellement avec l'application exemple et les scripts Python
5. **Soumettez** une Pull Request

## Règles

### Avant d'ouvrir une PR

- [ ] Le code compile sans warning (`cmake --build build`)
- [ ] Le serveur de démonstration fonctionne (`./build/servd`)
- [ ] Les clients Python de test passent
- [ ] La documentation est à jour
- [ ] `TODO.md` est mis à jour si nécessaire

### Style de commit

```
type(scope): description courte

Corps du message (optionnel)
```

Types : `feat`, `fix`, `docs`, `refactor`, `test`, `chore`

Exemples :
```
feat(router): ajout du support des wildcards
fix(engine): correction du timeout sur async_read
docs(api): mise à jour de la doc du Context
```

### PR

- Une PR = une fonctionnalité ou un bug fix
- Titre clair décrivant le changement
- Description expliquant le pourquoi et le comment
- Référencez l'issue associée

## Roadmap (v0.2+)

Voir [TODO.md](../TODO.md) pour la roadmap complète.

Priorités actuelles :
- Tests unitaires et d'intégration
- Découverte réseau fonctionnelle
- Documentation API complète
- Exemples supplémentaires

## Code de conduite

- Soyez respectueux et constructif
- Acceptez les critiques sur votre code
- Aidez les nouveaux contributeurs
