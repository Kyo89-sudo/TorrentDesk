# Contribuire

Eseguire build e test core/integrativi prima di proporre modifiche. I test di rete devono usare esclusivamente tracker locali controllati e torrent sintetici. Non allegare passkey, torrent privati o file di sessione nelle issue.

Per un profilo nuovo o aggiornato, riportare client, versione, OS, fonte ufficiale, data della verifica e differenze osservate in un ambiente di test autorizzato. Non dichiarare compatibilità basandosi solo sulla stringa User-Agent.

Il codice usa C++17 e API Windows; mantenere le richieste di rete fuori dal thread grafico e coprire parsing, contatori e ciclo di vita con test. Le modifiche vengono distribuite sotto Apache-2.0.
