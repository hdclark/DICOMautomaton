#!/bin/sh

psql -h localhost -d pacs -c "
SELECT 
    Parameters->>'ROIName' AS ROIName
  , Parameters->>'p' AS P
  , Parameters->>'q' AS Q
  , Parameters->>'r' AS R
--  , ((Parameters->>'p')::REAL + (Parameters->>'q')::REAL + (Parameters->>'r')::REAL) AS Order
  , moment AS Moment
  , Parameters->>'StudyInstanceUID' 
FROM
    moments_for_bigart2015 
WHERE
    ( 
        ( ((Parameters->>'p')::REAL + (Parameters->>'q')::REAL + (Parameters->>'r')::REAL) = 0)
      OR
        (
            ( ((Parameters->>'p')::REAL + (Parameters->>'q')::REAL + (Parameters->>'r')::REAL) = 3)
          AND
            (
                 ((Parameters->>'p')::REAL = 3)
              OR 
                 ((Parameters->>'q')::REAL = 3)
              OR
                 ((Parameters->>'r')::REAL = 3)
            )
        )
    )
  AND
    (Parameters->>'ROIName' ~* '_Parotid')
  AND
    (NOT( (Parameters->>'ROIName' ~* '_Rough') ))
  AND
    (NOT( (Parameters->>'ROIName' ~* '_ANT') ))
  AND
    (NOT( (Parameters->>'ROIName' ~* '_POST') ))
  AND
    (NOT( (Parameters->>'ROIName' ~* 'Carotid') ))
   --((Parameters->>'p' = '0') AND (Parameters->>'q' = '0') AND (Parameters->>'r' = '0'))
ORDER BY
    Parameters->>'ROIName'
  , Parameters->>'p'
  , Parameters->>'q'
  , Parameters->>'r'
  , Parameters->>'StudyInstanceUID'
"

