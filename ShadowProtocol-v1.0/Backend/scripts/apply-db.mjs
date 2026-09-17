import { readFile } from 'node:fs/promises';
import pg from 'pg';

const { Client } = pg;
const connectionString = process.env.DATABASE_URL;
const mode = process.argv[2] ?? 'init';

if (!connectionString) {
  throw new Error('DATABASE_URL is required for database initialization.');
}

const plans = {
  init: [
    new URL('../db/schema.sql', import.meta.url),
    new URL('../db/v101_connection_target.sql', import.meta.url),
    new URL('../db/v101_server_registry.sql', import.meta.url),
    new URL('../db/v101_node_credentials.sql', import.meta.url),
    new URL('../db/v101_node_credential_rotation.sql', import.meta.url)
  ],
  v101: [
    new URL('../db/v101_connection_target.sql', import.meta.url),
    new URL('../db/v101_server_registry.sql', import.meta.url),
    new URL('../db/v101_node_credentials.sql', import.meta.url),
    new URL('../db/v101_node_credential_rotation.sql', import.meta.url)
  ]
};

const files = plans[mode];
if (!files) {
  throw new Error(`Unknown database plan "${mode}". Use "init" or "v101".`);
}

const client = new Client({ connectionString });
await client.connect();

try {
  for (const fileUrl of files) {
    const sql = await readFile(fileUrl, 'utf8');
    await client.query('begin');
    try {
      await client.query(sql);
      await client.query('commit');
      console.log(`Applied ${fileUrl.pathname.split('/').pop()}`);
    } catch (error) {
      await client.query('rollback');
      throw error;
    }
  }
} finally {
  await client.end();
}
