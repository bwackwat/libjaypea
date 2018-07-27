
ALTER TABLE users ADD COLUMN color VARCHAR(20);

UPDATE users SET color = 'gold';

ALTER TABLE users ALTER COLUMN color SET NOT NULL;

ALTER TABLE users ALTER color SET DEFAULT 'gold';


7/26/2018


UPDATE access_types SET description = 'Has blogging privilege';
