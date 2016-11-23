# Compiler le projet

## Mandelbrot
$ make mandelbrot
$ ./bin/mandelbrot

## Producteur/Consommateur
### Compilation
$ make prod_conso
$ ./bin/prod_conso
### Arguments
-t [nombre de test]
-async [nb producteur] [nb consommateur] [taille du canal] [taille élément] [nb de send]
-sync [nb producteur] [nb consommateur] [taille élément] [nb send]
-lots [nb_producteur] [nb_consommateur] [taille du canal] [taille élément] [nb de send] [producteur envoi par lots]
    [producteur taille des lots] [consommateur recoit par lots] [consommateur taille des lots]
-tube [nb producteur] [nb consommateur] [taille élément] [nb de send]

## Brute Force
### Compilation
$ make brute_force
$ ./bin/brute_force [ARGS]

### Arguments
-s, --size : défini la taille du mdp
-t, --type : défini le type du mdp
  (n : nombre, l : minuscule,
   u : majuscule, a : tout type)
-d, --debug : affiche les informations

### Exemple d'utilisation
$ ./bin/brute_force
$ ./bin/brute_force -s 4 --debug
$ ./bin/brute_force -s 8 -t a