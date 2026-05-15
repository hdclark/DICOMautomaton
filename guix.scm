(use-modules (ice-9 ftw))

(add-to-load-path
  (string-append (dirname (current-filename)) "/.guix/modules"))

(use-modules (dicomautomaton packages))

dicomautomaton
