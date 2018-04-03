### Gestion des relays de charge et décharge dans un système de batterie Lifepo4.

Dans un système avec batteries au lifepo4, il faut pouvoir couper la charge quand les batteries sont pleines, pour ne pas passer en mode float et abimer les batteries sur le long terme.

Il faut aussi désactiver toute charge si les batteries ont atteint leur maximum de capacité de décharge (en général 80%).

J'ai choisi d'utiliser les relais Blue Sea ML-RBS qui ne consomme aucun courant.

Les relays classiques à Soleinoides sont plus faciles à gérer mais consomment, pour les meilleurs d'entre eux 0.1A 24/24h ce qui fait un total de 2.4Ah par jour et comme il en faut 2 on arrive à une consommation de 4.8Ah par jour.
Un relai classique consomme entre 0.3 et 1A, on arrive dans ce cas à des consommations non négligeables de 14Ah à 48Ah par jour, simplement pour des relais qui servent pour :
- Celui de charge (Charge) : couper les différentes charges et ne pas laisser la batterie Lifepo4 en mode float
- Celui de décharge (Load) : isoler de toute décharge la batterie Lifepo4 tout en laissant la possibilité de la charger.

Les relais Blue Sea ML-RBS ont aussi une sortie qui permet de connaitre l'état du relai (ouvert ou fermé). On peut récupérer cette information et la traiter avec l'Arduino.

Il faut au moins 2A en 12v pour ouvrir ou fermer le relai. L'arduino seul n'est pas capable de fournir une tel intensité ni un telle tension. J'utilise donc une cascade de transisteurs.

Le soucis majeur lors de la création de la carte a été que les relais sont cablés en High Side, c'est à dire que les solenoides se trouvent du côté positif du circuit ce qui implique d'utiliser une cascade de transistor pour les activer avec un système PNP + NPN.

Je me sers aussi du port de communication (VE Direct) du controleur Victron BMV702 pour récupérer l'état de charge de la batterie (SOC) ainsi que de leur tension.

J'ai ensuite pris le parti d'acquérir les tensions des 4 cellules que composent la batteries pour gérer les écarts de tensions et connaitre déterminer si elles sont pleines sans passer par le controleur victron.

L'arduino reçoit les informations suivantes : 
- Etat du relai de charge
- Etat du relai de décharge
- SOC via le controleur Victron
- Tension de la batterie via le Victron
- Tensions des différentes cellules

