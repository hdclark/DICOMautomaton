#!/bin/sh

psql -h localhost -d pacs -c "
SELECT 
    Parameters->>'ROIName' AS ROIName
  , Parameters->>'p' AS P
  , Parameters->>'q' AS Q
  , Parameters->>'r' AS R
--  , ((Parameters->>'p')::REAL + (Parameters->>'q')::REAL + (Parameters->>'r')::REAL) AS Order
--  , moment AS Moment
  , CASE WHEN (Parameters->>'StudyInstanceUID' = '1.3.46.670589.11.5720.5.0.5168.2015050810005704046')
              THEN (moment / (50896.1^2.0))::NUMERIC(4,4)
         WHEN (Parameters->>'StudyInstanceUID' = '1.3.46.670589.11.5720.5.0.6792.2015042411224164091')
              THEN (moment / (35582.4^2.0))::NUMERIC(4,4)
         WHEN (Parameters->>'StudyInstanceUID' = '1.3.46.670589.11.5720.5.0.6792.2015042414281765155')
              THEN (moment / (30989.1^2.0))::NUMERIC(4,4)
    END as InvariantMoment
  , CASE WHEN (Parameters->>'StudyInstanceUID' = '1.3.46.670589.11.5720.5.0.5168.2015050810005704046')
              THEN 'A'
         WHEN (Parameters->>'StudyInstanceUID' = '1.3.46.670589.11.5720.5.0.6792.2015042411224164091')
              THEN 'B'
         WHEN (Parameters->>'StudyInstanceUID' = '1.3.46.670589.11.5720.5.0.6792.2015042414281765155')
              THEN 'C'
    END as Volunteer
  --, Parameters->>'StudyInstanceUID' 
FROM
    moments_for_bigart2015 
WHERE
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

