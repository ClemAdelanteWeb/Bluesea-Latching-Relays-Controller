### Gestion des relays de charge et décharge dans un système de batterie Lifepo4.

Dans un système avec batteries au lifepo4, il faut pouvoir couper la charge quand les batteries sont pleines, pour ne pas passer en mode float et abimer les batteries sur le long terme.

Toute décharge doit être rendue impossible si les batteries ont atteint leur maximum de capacité de décharge (en général 80%), tout en laissant la charge se faire.
Il faut donc deux relais qui permettent de bloquer la charge, bloquer la décharge et bloquer les deux ce qui ferait l'équivalent d'un coupe circuit.

Il existe plusieurs sortent de relais. Les classiques à solenoide qui sont finalement constitués d'une lamelle à ressort que vient plaquer et fermer une bobine qui une fois alimentée crée un champ magnétique. Cette bobine c'est une solénoide, qu'on retrouve dans tous les relais classiques.

Il est aisé de déduire que ce type de relai consomme de l'énergie en permanence pour que le contact se fasse et le courant puisse passer. Pour les meilleurs d'entre eux 0.1A 24/24h ce qui fait un total de 2.4Ah par jour et comme il en faut 2 on arrive à une consommation de 4.8Ah par jour.

Un relai classique consomme entre 0.3 et 1A, on arrive dans ce cas à des consommations non négligeables de 14Ah à 48Ah par jour, simplement pour alimenter des relais qui servent à :
- Celui de charge (Charge) : couper les différentes charges et ne pas laisser la batterie Lifepo4 en mode float
- Celui de décharge (Load) : isoler de toute décharge profonde la batterie Lifepo4 tout en laissant la possibilité de la charger.

Les relais à solénoide optimisé (0.1A) sont très chers et même 5Ah par jour était de trop pour moi. 

Il existe aussi des relais mosfet qu'on trouve partout en électronique de puissance. S'il est plus compliquer d'estimer leur consommation lorsqu'ils sont passant, on comprend vite qu'elle est élevée puisqu'il faut un radiateur pour dissiper la chaleur qu'il produit.
Les mosfets sont aussi évincés de ma liste.

Heureusement il reste une solution : les relais bi-stable (ou latching relay en anglais).
Ceux-ci possèdent deux solénoides par relai. Un pour ouvrir le contact et un autre pour le fermer. Cette fois ci, et au contraire des relais classique, la position de la lamelle est permanente. C'est à dire qu'il n'y a pas besoin d'alimenter en permanence le solénoide pour créer un champ magnétique et forcer à lamelle à rester en place. Ici, comme pour un interrupteur à deux positions le relai ne consomme du courant que pour changer son état.

C'est parfait sauf que ça complique les choses puisqu'il y a deux bobines à alimenter par relai. Et comment savoir dans quelle position est le relai, ouvert ou fermé ?

J'ai choisi d'utiliser les relais Blue Sea ML-RBS qui ne consomment aucun courant et ont aussi une sortie qui permet de connaitre l'état du relai (ouvert ou fermé). On peut récupérer cette information et la traiter avec l'Arduino.

Il faut au moins 2A en 12v pour ouvrir ou fermer le relai. L'arduino seul n'est pas capable de fournir une tel intensité ni un telle tension. J'utilise donc une cascade de transisteurs.

Le soucis majeur lors de la création de la carte a été que les relais sont cablés en High Side, c'est à dire que les solenoides se trouvent du côté positif du circuit ce qui implique d'utiliser une cascade de transistor pour les activer avec un système PNP + NPN.

Pour déterminer le SOC (capacité restante dans la batterie en pourcentage) et l'utiliser pour gérer mes relais, j'utilise mon controleur de batterie Victron BMV702 qui fournit cette information par son port de communication (VE Direct). 

J'ai ensuite pris le parti d'acquérir les tensions des 4 cellules que composent la batteries pour controller les écarts entre les cellules, vérifier si l'une d'entre elles venait à être sous-chargée ou sur-chargée et connaitre la tension totale de la batterie.

Autant d'informations capitales qui permettent aussi de controller les relais sans les données fournies par le Victron, si celui-ci venait à tomber en panne.

J'ai aussi ajouté un bouton ON/OFF pour désactiver l'utilisation du SOC venant du Victron.

L'arduino reçoit donc les informations suivantes : 
- Etat du relai de charge
- Etat du relai de décharge
- SOC via le controleur Victron
- Tension de chaque cellule
- Utilisation du Victron ON / OFF

Par la suite, je vais créer un petit écran pour afficher toutes les données (voltage des cellules, état des relais, cycle de charge ou cycle de décharge, sur-tension ou sous-tension détectée). 

Pour des batteries au lithium, il n'est pas recommandé de maintenir la charge à 100% (mode float).
Aujourd'hui le programme ne possède qu'un cycle de charge qui va de 95 à 100% du SOC. Un cycle se compose comme ceci : la batterie se charge jusqu'à 100%, puis la charge est bloquée jusqu'à ce que la batterie se vide de 5%. A 95% de SOC, donc, la charge est de nouveau autorisée. 

Pour prolonger la durée de vie des batteries lithium, le SOC idéal est entre 50 et 70%. Ce serait intéressant de pouvoir faire des cycles entre 50 et 70% lorsque l'on est au port par exemple et qu'on n'a pas besoin de toute la capacité de la batterie puisqu'elle est rechargée au quai. 
Une autre utilisation de ce cycle 50-70% pourrait être faite en hivernage, période pendant laquelle la batterie n'est pas solicitée mais que la charge peut être faite par un panneau solaire par exemple. Grâce au controleur il serait possible de laisser tourner un petit ventilateur 12V dans les endroits qui en auraient besoin, par exemple, en aillant la certitude qu'il ne déchargera pas la batterie en dessous de 50%.

Une fois que l'autonomie totale est requise on retourne sur le cycle normal qui est de 95 à 100%.




