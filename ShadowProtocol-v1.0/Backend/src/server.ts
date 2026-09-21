import { exchangeReconnect, admitReconnect } from './reconnect-admission.js';
import Fastify from 'fastify';
import cors from '@fastify/cors';
import websocket from '@fastify/websocket';
import { Pool } from 'pg';
import { Redis } from 'ioredis';
import { z } from 'zod';
import { createHash, createHmac, randomBytes, timingSafeEqual } from 'node:crypto';

const app = Fastify({ logger: true, trustProxy: true });
await app.register(cors, { origin: false });
await app.register(websocket);

const pool = process.env.DATABASE_URL ? new Pool({ connectionString: process.env.DATABASE_URL }) : null;
const redis = process.env.REDIS_URL ? new Redis(process.env.REDIS_URL, { lazyConnect: true }) : null;

const GAME_RELEASE = '1.0.1';
const NETWORK_BUILD = 'SP-1.0.1';
const CONTENT_REVISION = 'EMBASSY-PROTOCOL-101';
const BACKEND_PROTOCOL_VERSION = '0.8.0';
const SESSION_TTL_MS = 15 * 60_000;
const SESSION_TTL_SECONDS = Math.floor(SESSION_TTL_MS / 1000);
const IS_PRODUCTION = process.env.NODE_ENV === 'production';
const GAME_SERVER_PUBLIC_HOST = (process.env.GAME_SERVER_PUBLIC_HOST ?? (IS_PRODUCTION ? '' : '127.0.0.1')).trim();
const GAME_SERVER_PUBLIC_PORT = Number(process.env.GAME_SERVER_PUBLIC_PORT ?? (IS_PRODUCTION ? '0' : '7777'));
const heartbeatTtlCandidate = Number(process.env.SERVER_HEARTBEAT_TTL_MS ?? 30_000);
const SERVER_HEARTBEAT_TTL_MS = Number.isFinite(heartbeatTtlCandidate)
  ? Math.min(300_000, Math.max(5_000, Math.trunc(heartbeatTtlCandidate)))
  : 30_000;
const credentialTtlCandidate = Number(process.env.NODE_CREDENTIAL_TTL_MS ?? 21_600_000);
const NODE_CREDENTIAL_TTL_MS = Number.isFinite(credentialTtlCandidate)
  ? Math.min(604_800_000, Math.max(300_000, Math.trunc(credentialTtlCandidate)))
  : 21_600_000;
const credentialGraceCandidate = Number(process.env.NODE_CREDENTIAL_GRACE_MS ?? 120_000);
const NODE_CREDENTIAL_GRACE_MS = Number.isFinite(credentialGraceCandidate)
  ? Math.min(Math.min(600_000, Math.floor(NODE_CREDENTIAL_TTL_MS / 2)), Math.max(5_000, Math.trunc(credentialGraceCandidate)))
  : 120_000;
const REQUIRE_SERVER_ATTESTATION = (process.env.REQUIRE_SERVER_ATTESTATION ?? (IS_PRODUCTION && Boolean(pool) ? 'true' : 'false')).trim().toLowerCase() === 'true';
const ORCHESTRATOR_ATTESTATION_SECRET = (process.env.ORCHESTRATOR_ATTESTATION_SECRET ?? '').trim();
const attestationTtlCandidate = Number(process.env.SERVER_ATTESTATION_MAX_TTL_MS ?? 120_000);
const SERVER_ATTESTATION_MAX_TTL_MS = Number.isFinite(attestationTtlCandidate)
  ? Math.min(300_000, Math.max(30_000, Math.trunc(attestationTtlCandidate)))
  : 120_000;
const SERVER_ATTESTATION_CLOCK_SKEW_MS = 15_000;
const SERVER_ATTESTATION_ISSUER = 'shadow-protocol-orchestrator';
const SERVER_ATTESTATION_AUDIENCE = 'shadow-protocol-server-registration';
const ACCEPTED_NETWORK_BUILDS = new Set(
  (process.env.ACCEPTED_NETWORK_BUILDS ?? NETWORK_BUILD)
    .split(',')
    .map((build) => build.trim())
    .filter(Boolean)
);
const isBuildCompatible = (build: string) => ACCEPTED_NETWORK_BUILDS.has(build.trim());
const compatibilityPayload = () => ({
  gameRelease: GAME_RELEASE,
  networkBuild: NETWORK_BUILD,
  contentRevision: CONTENT_REVISION,
  backendProtocol: BACKEND_PROTOCOL_VERSION,
  acceptedNetworkBuilds: [...ACCEPTED_NETWORK_BUILDS],
  enforcement: 'strict'
});

const SESSION_SECRET = process.env.SESSION_SIGNING_SECRET ?? 'dev-only-change-me';
const SESSION_BOOTSTRAP_SECRET = process.env.SESSION_BOOTSTRAP_SECRET ?? 'dev-bootstrap-change-me';
const SERVER_REGISTRATION_SECRET = process.env.SERVER_REGISTRATION_SECRET ?? process.env.MATCH_SERVER_SECRET ?? 'dev-match-secret';
const b64url=(value:string|Buffer)=>Buffer.from(value).toString('base64url');
const sha256=(value:string)=>createHash('sha256').update(value).digest('hex');
type SessionPayload={sid:string;uid:string;exp:number;region:string;build:string;protocol:string};
type ServerAttestationPayload={
  jti:string;iss:string;aud:string;iat:number;exp:number;serverId:string;region:string;networkBuild:string;
  publicHost:string;publicPort:number;capacity:number;
};
type ConnectTarget={host:string;port:number};
type RegisteredServerTarget=ConnectTarget&{nodeId:string;serverId:string;capacity:number;activeAllocations:number};
type AuthenticatedServerNode={nodeId:string;serverId:string;region:string;networkBuild:string;status:string;authSlot:'current'|'previous'};
function signSession(payload:Record<string,unknown>){
  const encoded=b64url(JSON.stringify(payload));
  const sig=createHmac('sha256',SESSION_SECRET).update(encoded).digest('base64url');
  return `${encoded}.${sig}`;
}
function verifySession(token:string){
  const [encoded,sig]=token.split('.');if(!encoded||!sig)return null;
  const expected=createHmac('sha256',SESSION_SECRET).update(encoded).digest();
  let supplied:Buffer;try{supplied=Buffer.from(sig,'base64url')}catch{return null}
  if(expected.length!==supplied.length||!timingSafeEqual(expected,supplied))return null;
  try{
    const payload=JSON.parse(Buffer.from(encoded,'base64url').toString('utf8')) as SessionPayload;
    if(payload.exp<Date.now()||!payload.sid||!payload.uid||!payload.region||!payload.build||!payload.protocol)return null;
    return payload;
  }catch{return null}
}
function bearer(req:any){const h=String(req.headers?.authorization??'');return h.startsWith('Bearer ')?h.slice(7):''}
function incompatibleBuild(reply:any, receivedBuild:string|null){
  return reply.code(426).send({error:'client-build-incompatible',receivedBuild,...compatibilityPayload()});
}
function requireServerRegistrationBootstrap(req:any, reply:any){
  if(String(req.headers['x-match-server-secret']??'')!==SERVER_REGISTRATION_SECRET){
    reply.code(401).send({error:'server-registration-auth-required'});
    return false;
  }
  return true;
}
const serverAttestationClaimsSchema=z.object({
  jti:z.string().uuid(),iss:z.literal(SERVER_ATTESTATION_ISSUER),aud:z.literal(SERVER_ATTESTATION_AUDIENCE),
  iat:z.number().int().positive(),exp:z.number().int().positive(),
  serverId:z.string().min(2).max(64),region:z.string().min(2).max(16),networkBuild:z.string().min(2).max(32),
  publicHost:z.string().trim().min(1).max(255),publicPort:z.number().int().min(1).max(65535),capacity:z.number().int().min(1).max(128)
});
function verifyServerAttestation(token:string):ServerAttestationPayload|null{
  if(!token||!ORCHESTRATOR_ATTESTATION_SECRET)return null;
  const [encoded,sig]=token.split('.');if(!encoded||!sig)return null;
  const expected=createHmac('sha256',ORCHESTRATOR_ATTESTATION_SECRET).update(encoded).digest();
  let supplied:Buffer;try{supplied=Buffer.from(sig,'base64url')}catch{return null}
  if(expected.length!==supplied.length||!timingSafeEqual(expected,supplied))return null;
  try{
    const raw=JSON.parse(Buffer.from(encoded,'base64url').toString('utf8'));
    const parsed=serverAttestationClaimsSchema.safeParse(raw);if(!parsed.success)return null;
    const claims=parsed.data,now=Date.now();
    if(claims.iat>now+SERVER_ATTESTATION_CLOCK_SKEW_MS)return null;
    if(claims.exp<=now-SERVER_ATTESTATION_CLOCK_SKEW_MS)return null;
    if(claims.exp<=claims.iat||claims.exp-claims.iat>SERVER_ATTESTATION_MAX_TTL_MS)return null;
    if(claims.iat<now-SERVER_ATTESTATION_MAX_TTL_MS-SERVER_ATTESTATION_CLOCK_SKEW_MS)return null;
    return claims;
  }catch{return null}
}
function attestationMatchesRegistration(attestation:ServerAttestationPayload,registration:{
  serverId:string;region:string;networkBuild:string;publicHost:string;publicPort:number;capacity:number;
}){
  return attestation.serverId===registration.serverId
    && attestation.region===registration.region
    && attestation.networkBuild===registration.networkBuild
    && attestation.publicHost===registration.publicHost
    && attestation.publicPort===registration.publicPort
    && attestation.capacity===registration.capacity;
}
function secureHashMatches(storedHash:string,suppliedSecret:string){
  const suppliedHash=sha256(suppliedSecret);
  const stored=Buffer.from(storedHash);
  const supplied=Buffer.from(suppliedHash);
  return stored.length===supplied.length&&timingSafeEqual(stored,supplied);
}
async function requireNodeCredential(req:any,reply:any):Promise<AuthenticatedServerNode|null>{
  const serverId=String(req.headers['x-sp-server-id']??'').trim();
  const nodeCredential=String(req.headers['x-sp-node-credential']??'').trim();
  if(!serverId||!nodeCredential){reply.code(401).send({error:'node-auth-required'});return null;}
  if(!pool){reply.code(503).send({error:'database-not-configured'});return null;}
  const r=await pool.query(`select id as node_id,server_id,region,network_build,status,credential_hash,credential_expires_at,
      previous_credential_hash,previous_credential_valid_until
    from game_server_nodes where server_id=$1 and credential_revoked_at is null and status<>'offline'`,[serverId]);
  if(!r.rowCount){reply.code(401).send({error:'node-auth-required'});return null;}
  const node=r.rows[0],now=Date.now();
  const currentMatches=Boolean(node.credential_hash)&&secureHashMatches(String(node.credential_hash),nodeCredential);
  if(currentMatches){
    const expiresAt=node.credential_expires_at?new Date(node.credential_expires_at).getTime():0;
    if(expiresAt<=now){reply.code(401).send({error:'node-credential-expired'});return null;}
    return {nodeId:node.node_id,serverId:node.server_id,region:node.region,networkBuild:node.network_build,status:node.status,authSlot:'current'};
  }
  const previousMatches=Boolean(node.previous_credential_hash)&&secureHashMatches(String(node.previous_credential_hash),nodeCredential);
  const previousValidUntil=node.previous_credential_valid_until?new Date(node.previous_credential_valid_until).getTime():0;
  if(previousMatches&&previousValidUntil>now){
    return {nodeId:node.node_id,serverId:node.server_id,region:node.region,networkBuild:node.network_build,status:node.status,authSlot:'previous'};
  }
  reply.code(401).send({error:'node-auth-required'});return null;
}
async function requireMatchServer(req:any,reply:any):Promise<boolean>{
  const node=await requireNodeCredential(req,reply);if(!node)return false;
  const body=(req.body??{}) as Record<string,unknown>;
  const requestedServerId=typeof body.serverId==='string'?body.serverId.trim():'';
  if(requestedServerId&&requestedServerId!==node.serverId){reply.code(403).send({error:'node-identity-mismatch'});return false;}
  const matchId=typeof body.matchId==='string'?body.matchId.trim():'';
  if(matchId){
    if(!z.string().uuid().safeParse(matchId).success){reply.code(400).send({error:'invalid-match-id'});return false;}
    const owned=await pool!.query(`select 1 from server_allocations where match_id=$1 and node_id=$2 and status not in ('closed','failed') limit 1`,[matchId,node.nodeId]);
    if(!owned.rowCount){reply.code(403).send({error:'match-node-ownership-required'});return false;}
  }
  return true;
}
function requireCompatibleSession(req:any, reply:any):SessionPayload|null{
  const session=verifySession(bearer(req));
  if(!session){reply.code(401).send({error:'invalid-game-session'});return null;}
  if(!isBuildCompatible(session.build)||session.protocol!==BACKEND_PROTOCOL_VERSION){incompatibleBuild(reply,session.build);return null;}
  return session;
}
function getStaticConnectTarget():ConnectTarget|null{
  if(!GAME_SERVER_PUBLIC_HOST||!Number.isInteger(GAME_SERVER_PUBLIC_PORT)||GAME_SERVER_PUBLIC_PORT<1||GAME_SERVER_PUBLIC_PORT>65535)return null;
  return {host:GAME_SERVER_PUBLIC_HOST,port:GAME_SERVER_PUBLIC_PORT};
}
function requireConnectTarget(reply:any):ConnectTarget|null{
  const target=getStaticConnectTarget();
  if(!target){reply.code(503).send({error:'game-server-connect-target-not-configured'});return null;}
  return target;
}
async function cleanupExpiredServerReservations(client:any){
  await client.query(`with expired as (
    update server_allocations
    set status='failed',ended_at=coalesce(ended_at,now())
    where node_id is not null and status in ('reserved','starting','ready') and expires_at<=now()
    returning node_id
  ), released as (
    select node_id,count(*)::int as release_count from expired where node_id is not null group by node_id
  )
  update game_server_nodes n
  set active_allocations=greatest(0,n.active_allocations-r.release_count),updated_at=now()
  from released r where n.id=r.node_id`);

  await client.query(`update matches m
    set assembly_state='assembling',assembled_at=null
    where m.assembly_state='ready' and m.ended_at is null
      and not exists(select 1 from match_rounds mr where mr.match_id=m.id)
      and (select count(*) from server_allocations sa
        where sa.match_id=m.id and sa.status not in ('closed','failed'))<m.target_players`);
}
async function reserveRegisteredServer(client:any,region:string,build:string):Promise<RegisteredServerTarget|null>{
  for(let attempt=0;attempt<3;attempt+=1){
    const r=await client.query(`with candidate as (
      select id from game_server_nodes
      where region=$1 and network_build=$2 and status='ready'
        and credential_revoked_at is null and credential_expires_at>now()
        and last_heartbeat_at>now()-($3::double precision*interval '1 millisecond')
        and active_allocations<capacity
      order by active_allocations::numeric/nullif(capacity,0),last_heartbeat_at desc,server_id
      limit 1
    )
    update game_server_nodes n
    set active_allocations=n.active_allocations+1,updated_at=now()
    from candidate c
    where n.id=c.id and n.active_allocations<n.capacity
    returning n.id as node_id,n.server_id,n.public_host,n.public_port,n.capacity,n.active_allocations`,[region,build,SERVER_HEARTBEAT_TTL_MS]);
    if(r.rowCount){
      const node=r.rows[0];
      return {nodeId:node.node_id,serverId:node.server_id,host:node.public_host,port:Number(node.public_port),capacity:Number(node.capacity),activeAllocations:Number(node.active_allocations)};
    }
  }
  return null;
}
async function reserveSpecificRegisteredServer(client:any,nodeId:string):Promise<RegisteredServerTarget|null>{
  const r=await client.query(`update game_server_nodes
    set active_allocations=active_allocations+1,updated_at=now()
    where id=$1 and status='ready'
      and credential_revoked_at is null and credential_expires_at>now()
      and last_heartbeat_at>now()-($2::double precision*interval '1 millisecond')
      and active_allocations<capacity
    returning id as node_id,server_id,public_host,public_port,capacity,active_allocations`,[nodeId,SERVER_HEARTBEAT_TTL_MS]);
  if(!r.rowCount)return null;
  const node=r.rows[0];
  return {nodeId:node.node_id,serverId:node.server_id,host:node.public_host,port:Number(node.public_port),capacity:Number(node.capacity),activeAllocations:Number(node.active_allocations)};
}

app.get('/health', async () => ({
  service: 'shadow-protocol-backend', ok: true, version: BACKEND_PROTOCOL_VERSION,
  serverRegistry: { enabled:Boolean(pool), heartbeatTtlMs:SERVER_HEARTBEAT_TTL_MS, productionRequiresHealthyNode:IS_PRODUCTION,
    nodeCredentialTtlMs:NODE_CREDENTIAL_TTL_MS,nodeCredentialGraceMs:NODE_CREDENTIAL_GRACE_MS,
    attestationRequired:REQUIRE_SERVER_ATTESTATION,attestationVerifierConfigured:Boolean(ORCHESTRATOR_ATTESTATION_SECRET),
    attestationMaxTtlMs:SERVER_ATTESTATION_MAX_TTL_MS },
  ...compatibilityPayload()
}));
app.get('/v1/compatibility', async () => compatibilityPayload());

const serverRegistrationSchema=z.object({
  serverId:z.string().min(2).max(64).regex(/^[A-Za-z0-9][A-Za-z0-9._-]+$/),
  region:z.string().min(2).max(16),networkBuild:z.string().min(2).max(32),publicHost:z.string().trim().min(1).max(255),
  publicPort:z.number().int().min(1).max(65535),capacity:z.number().int().min(1).max(128).default(1)
});
app.post('/v1/servers/register',async(req,reply)=>{
  if(!requireServerRegistrationBootstrap(req,reply))return;
  const parsed=serverRegistrationSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(!pool)return reply.code(503).send({error:'database-not-configured'});
  if(!isBuildCompatible(parsed.data.networkBuild))return reply.code(409).send({error:'server-build-incompatible',receivedBuild:parsed.data.networkBuild,...compatibilityPayload()});

  const attestationToken=String(req.headers['x-sp-node-attestation']??'').trim();
  let attestation:ServerAttestationPayload|null=null;
  if(REQUIRE_SERVER_ATTESTATION||attestationToken){
    if(!attestationToken)return reply.code(401).send({error:'server-attestation-required'});
    if(!ORCHESTRATOR_ATTESTATION_SECRET)return reply.code(503).send({error:'server-attestation-verifier-not-configured'});
    attestation=verifyServerAttestation(attestationToken);
    if(!attestation)return reply.code(401).send({error:'server-attestation-invalid'});
    if(!attestationMatchesRegistration(attestation,parsed.data))return reply.code(403).send({error:'server-attestation-mismatch'});
  }

  const nodeCredential=randomBytes(32).toString('base64url'),credentialHash=sha256(nodeCredential),credentialExpiresAt=Date.now()+NODE_CREDENTIAL_TTL_MS;
  const client=await pool.connect();
  try{
    await client.query('begin');
    if(attestation){
      await client.query(`delete from server_node_attestations where expires_at<now()-interval '24 hours'`);
      const consumed=await client.query(`insert into server_node_attestations(id,server_id,region,network_build,issued_at,expires_at,consumed_at)
        values($1,$2,$3,$4,to_timestamp($5/1000.0),to_timestamp($6/1000.0),now())
        on conflict(id) do nothing returning id`,
        [attestation.jti,attestation.serverId,attestation.region,attestation.networkBuild,attestation.iat,attestation.exp]);
      if(!consumed.rowCount){await client.query('rollback');return reply.code(409).send({error:'server-attestation-replayed'});}
    }
    const r=await client.query(`insert into game_server_nodes(server_id,region,network_build,public_host,public_port,status,capacity,last_heartbeat_at,credential_hash,credential_issued_at,credential_expires_at,credential_revoked_at,previous_credential_hash,previous_credential_valid_until,last_attestation_id,last_attested_at)
      values($1,$2,$3,$4,$5,'ready',$6,now(),$7,now(),to_timestamp($8/1000.0),null,null,null,$9,case when $9::uuid is null then null else now() end)
      on conflict(server_id) do update set region=excluded.region,network_build=excluded.network_build,public_host=excluded.public_host,
        public_port=excluded.public_port,capacity=excluded.capacity,status='ready',last_heartbeat_at=now(),credential_hash=excluded.credential_hash,
        credential_issued_at=now(),credential_expires_at=excluded.credential_expires_at,credential_revoked_at=null,
        previous_credential_hash=null,previous_credential_valid_until=null,last_attestation_id=excluded.last_attestation_id,
        last_attested_at=excluded.last_attested_at,updated_at=now()
      returning id as node_id,server_id,region,network_build,public_host,public_port,status,capacity,active_allocations,last_heartbeat_at,credential_expires_at,last_attested_at`,
      [parsed.data.serverId,parsed.data.region,parsed.data.networkBuild,parsed.data.publicHost,parsed.data.publicPort,parsed.data.capacity,credentialHash,credentialExpiresAt,attestation?.jti??null]);
    await client.query('commit');
    return reply.send({...r.rows[0],heartbeatTtlMs:SERVER_HEARTBEAT_TTL_MS,nodeCredential,credentialTtlMs:NODE_CREDENTIAL_TTL_MS,
      credentialGraceMs:NODE_CREDENTIAL_GRACE_MS,credentialExpiresAt:new Date(credentialExpiresAt).toISOString(),
      attested:Boolean(attestation),attestationConsumed:Boolean(attestation)});
  }catch(error){await client.query('rollback');throw error;}finally{client.release();}
});

const serverRotateCredentialSchema=z.object({serverId:z.string().min(2).max(64)});
app.post('/v1/servers/rotate-credential',async(req,reply)=>{
  const node=await requireNodeCredential(req,reply);if(!node)return;
  const parsed=serverRotateCredentialSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(parsed.data.serverId!==node.serverId)return reply.code(403).send({error:'node-identity-mismatch'});
  if(node.authSlot!=='current')return reply.code(409).send({error:'node-credential-already-rotated'});
  const suppliedCredential=String(req.headers['x-sp-node-credential']??'');
  const suppliedHash=sha256(suppliedCredential);
  const nodeCredential=randomBytes(32).toString('base64url'),credentialHash=sha256(nodeCredential);
  const credentialExpiresAt=Date.now()+NODE_CREDENTIAL_TTL_MS,previousValidUntil=Date.now()+NODE_CREDENTIAL_GRACE_MS;
  const r=await pool!.query(`update game_server_nodes
    set previous_credential_hash=credential_hash,previous_credential_valid_until=to_timestamp($4/1000.0),
        credential_hash=$2,credential_issued_at=now(),credential_expires_at=to_timestamp($3/1000.0),updated_at=now()
    where id=$1 and credential_hash=$5 and credential_revoked_at is null
    returning server_id,credential_expires_at,previous_credential_valid_until`,
    [node.nodeId,credentialHash,credentialExpiresAt,previousValidUntil,suppliedHash]);
  if(!r.rowCount)return reply.code(409).send({error:'node-credential-rotation-raced'});
  return reply.send({serverId:node.serverId,nodeCredential,credentialTtlMs:NODE_CREDENTIAL_TTL_MS,
    credentialExpiresAt:new Date(credentialExpiresAt).toISOString(),previousCredentialValidUntil:new Date(previousValidUntil).toISOString()});
});

const serverHeartbeatSchema=z.object({serverId:z.string().min(2).max(64),status:z.enum(['ready','draining']).optional()});
app.post('/v1/servers/heartbeat',async(req,reply)=>{
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=serverHeartbeatSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(!pool)return reply.code(503).send({error:'database-not-configured'});
  const r=await pool.query(`update game_server_nodes set last_heartbeat_at=now(),status=coalesce($2,status),updated_at=now()
    where server_id=$1 returning server_id,region,network_build,status,capacity,active_allocations,last_heartbeat_at,credential_expires_at`,[parsed.data.serverId,parsed.data.status??null]);
  if(!r.rowCount)return reply.code(404).send({error:'server-node-not-registered'});
  return reply.send(r.rows[0]);
});

const serverNodeSchema=z.object({serverId:z.string().min(2).max(64)});
app.post('/v1/servers/drain',async(req,reply)=>{
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=serverNodeSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(!pool)return reply.code(503).send({error:'database-not-configured'});
  const r=await pool.query(`update game_server_nodes set status='draining',updated_at=now() where server_id=$1
    returning server_id,status,active_allocations,last_heartbeat_at`,[parsed.data.serverId]);
  if(!r.rowCount)return reply.code(404).send({error:'server-node-not-registered'});
  return reply.send(r.rows[0]);
});

const allocationReleaseSchema=z.object({allocationId:z.string().uuid(),matchId:z.string().uuid(),outcome:z.enum(['closed','failed']).default('closed')});
app.post('/v1/servers/release-allocation',async(req,reply)=>{
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=allocationReleaseSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(!pool)return reply.code(503).send({error:'database-not-configured'});
  const client=await pool.connect();
  try{
    await client.query('begin');
    const released=await client.query(`update server_allocations set status=$3,ended_at=coalesce(ended_at,now())
      where id=$1 and match_id=$2 and status not in ('closed','failed')
      returning node_id,server_id,status`,[parsed.data.allocationId,parsed.data.matchId,parsed.data.outcome]);
    if(!released.rowCount){await client.query('rollback');return reply.code(409).send({error:'allocation-already-released-or-missing'});}
    const allocation=released.rows[0];
    let activeAllocations:number|null=null;
    if(allocation.node_id){
      const node=await client.query(`update game_server_nodes set active_allocations=greatest(0,active_allocations-1),updated_at=now()
        where id=$1 returning active_allocations`,[allocation.node_id]);
      if(node.rowCount)activeAllocations=Number(node.rows[0].active_allocations);
    }
    await client.query('commit');
    return reply.send({released:true,allocationId:parsed.data.allocationId,matchId:parsed.data.matchId,serverId:allocation.server_id,status:allocation.status,activeAllocations});
  }catch(error){await client.query('rollback');throw error;}finally{client.release();}
});

const gameSessionSchema=z.object({userId:z.string().uuid(),region:z.string().min(2).max(16),build:z.string().min(2).max(32),deviceNonce:z.string().min(8).max(128)});
app.post('/v1/auth/game-session',async(req,reply)=>{
  // Production identity provider / platform auth calls this bootstrap endpoint. A raw userId from a game client is not sufficient identity proof.
  if(String(req.headers['x-session-bootstrap-secret']??'')!==SESSION_BOOTSTRAP_SECRET)return reply.code(401).send({error:'bootstrap-auth-required'});
  const parsed=gameSessionSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(!isBuildCompatible(parsed.data.build))return incompatibleBuild(reply,parsed.data.build);
  const sessionId=crypto.randomUUID(),expiresAt=Date.now()+SESSION_TTL_MS;
  const token=signSession({sid:sessionId,uid:parsed.data.userId,region:parsed.data.region,build:parsed.data.build,protocol:BACKEND_PROTOCOL_VERSION,exp:expiresAt});
  if(pool)await pool.query(`insert into game_sessions(id,user_id,region,build,device_nonce_hash,expires_at) values($1,$2,$3,$4,$5,to_timestamp($6/1000.0))`,[sessionId,parsed.data.userId,parsed.data.region,parsed.data.build,sha256(parsed.data.deviceNonce),expiresAt]);
  if(redis)await redis.setex(`game-session:${sessionId}`,SESSION_TTL_SECONDS,JSON.stringify({userId:parsed.data.userId,region:parsed.data.region,build:parsed.data.build,protocol:BACKEND_PROTOCOL_VERSION}));
  return reply.code(201).send({sessionId,sessionToken:token,expiresAt:new Date(expiresAt).toISOString(),authority:'authenticated-session',compatibility:compatibilityPayload()});
});

app.post('/v1/auth/refresh',async(req,reply)=>{
  const session=requireCompatibleSession(req,reply);if(!session)return;
  const expiresAt=Date.now()+SESSION_TTL_MS;
  if(pool){
    const r=await pool.query(`update game_sessions set expires_at=to_timestamp($3/1000.0) where id=$1 and user_id=$2 and expires_at>now() returning id`,[session.sid,session.uid,expiresAt]);
    if(!r.rowCount)return reply.code(403).send({error:'session-refresh-denied'});
  }
  const token=signSession({...session,exp:expiresAt});
  if(redis)await redis.setex(`game-session:${session.sid}`,SESSION_TTL_SECONDS,JSON.stringify({userId:session.uid,region:session.region,build:session.build,protocol:session.protocol}));
  return reply.send({sessionId:session.sid,sessionToken:token,region:session.region,expiresAt:new Date(expiresAt).toISOString(),compatibility:compatibilityPayload()});
});

const allocationSchema=z.object({region:z.string().min(2).max(16),mode:z.literal('PROTOCOL'),map:z.literal('EMBASSY'),ranked:z.boolean().default(true)});
const PROTOCOL_TARGET_PLAYERS=10;
app.post('/v1/matches/allocate',async(req,reply)=>{
  const session=requireCompatibleSession(req,reply);if(!session)return;
  const parsed=allocationSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(parsed.data.region!==session.region)return reply.code(409).send({error:'session-region-mismatch'});

  const allocationId=crypto.randomUUID();
  const connectToken=randomBytes(24).toString('base64url'),connectHash=sha256(connectToken),expiresAt=Date.now()+120_000;

  if(pool){
    const client=await pool.connect();
    try{
      await client.query('begin');
      const assemblyKey=`${parsed.data.region}|${session.build}|${parsed.data.mode}|${parsed.data.map}|${parsed.data.ranked?'1':'0'}`;
      await client.query('select pg_advisory_xact_lock(hashtext($1))',[assemblyKey]);
      await cleanupExpiredServerReservations(client);

      const existingAllocation=await client.query(`select sa.id,sa.match_id
        from server_allocations sa join matches m on m.id=sa.match_id
        where sa.user_id=$1 and sa.status not in ('closed','failed') and m.ended_at is null
        limit 1`,[session.uid]);
      if(existingAllocation.rowCount){
        await client.query('rollback');
        return reply.code(409).send({error:'active-allocation-exists',allocationId:existingAllocation.rows[0].id,matchId:existingAllocation.rows[0].match_id});
      }

      let matchId:string|null=null;
      let connectTarget:ConnectTarget|null=null;
      let serverId='';
      let nodeId:string|null=null;
      let allocator:'registry'|'static-dev'='registry';

      const assembling=await client.query(`select m.id as match_id,m.target_players,
          min(sa.server_id) as server_id,min(sa.node_id::text)::uuid as node_id,
          min(sa.connect_host) as connect_host,min(sa.connect_port) as connect_port,
          count(sa.id)::int as reserved_players
        from matches m
        join server_allocations sa on sa.match_id=m.id and sa.status not in ('closed','failed')
        where m.mode=$1 and m.map_code=$2 and m.region=$3 and m.ranked=$4 and m.server_build=$5
          and m.assembly_state='assembling' and m.ended_at is null
        group by m.id,m.target_players,m.created_at
        having count(sa.id)<m.target_players
        order by m.created_at asc
        limit 1`,[parsed.data.mode,parsed.data.map,parsed.data.region,parsed.data.ranked,session.build]);

      if(assembling.rowCount&&assembling.rows[0].node_id){
        const reserved=await reserveSpecificRegisteredServer(client,assembling.rows[0].node_id);
        if(reserved){
          matchId=assembling.rows[0].match_id;
          connectTarget={host:reserved.host,port:reserved.port};
          serverId=reserved.serverId;
          nodeId=reserved.nodeId;
        }
      }

      if(!matchId){
        const registered=await reserveRegisteredServer(client,parsed.data.region,session.build);
        if(registered){
          connectTarget={host:registered.host,port:registered.port};
          serverId=registered.serverId;
          nodeId=registered.nodeId;
          matchId=crypto.randomUUID();
          await client.query(`insert into matches(id,mode,map_code,region,ranked,server_build,assembly_state,target_players)
            values($1,$2,$3,$4,$5,$6,'assembling',$7)`,
            [matchId,parsed.data.mode,parsed.data.map,parsed.data.region,parsed.data.ranked,session.build,PROTOCOL_TARGET_PLAYERS]);
        }else{
          if(IS_PRODUCTION){await client.query('rollback');return reply.code(503).send({error:'no-healthy-game-server',region:parsed.data.region,networkBuild:session.build,heartbeatTtlMs:SERVER_HEARTBEAT_TTL_MS});}
          const fallback=getStaticConnectTarget();
          if(!fallback){await client.query('rollback');return reply.code(503).send({error:'game-server-connect-target-not-configured'});}
          connectTarget=fallback;
          serverId=`${parsed.data.region.toUpperCase()}-DEV-${randomBytes(2).toString('hex').toUpperCase()}`;
          allocator='static-dev';
          matchId=crypto.randomUUID();
          await client.query(`insert into matches(id,mode,map_code,region,ranked,server_build,assembly_state,target_players)
            values($1,$2,$3,$4,$5,$6,'ready',1)`,
            [matchId,parsed.data.mode,parsed.data.map,parsed.data.region,parsed.data.ranked,session.build]);
        }
      }

      if(!connectTarget||!matchId)throw new Error('match assembly did not produce a connection target');

      await client.query(`insert into server_allocations(id,match_id,server_id,region,status,user_id,connect_token_hash,connect_host,connect_port,expires_at,node_id)
        values($1,$2,$3,$4,'reserved',$5,$6,$7,$8,to_timestamp($9/1000.0),$10)`,
        [allocationId,matchId,serverId,parsed.data.region,session.uid,connectHash,connectTarget.host,connectTarget.port,expiresAt,nodeId]);

      const assembly=await client.query(`select m.target_players,count(sa.id)::int as reserved_players
        from matches m
        join server_allocations sa on sa.match_id=m.id and sa.status not in ('closed','failed')
        where m.id=$1 group by m.id,m.target_players`,[matchId]);
      const targetPlayers=Number(assembly.rows[0]?.target_players??1);
      const reservedPlayers=Number(assembly.rows[0]?.reserved_players??1);
      const assemblyState=reservedPlayers>=targetPlayers?'ready':'assembling';
      await client.query(`update matches set assembly_state=$2,assembled_at=case when $2='ready' then coalesce(assembled_at,now()) else assembled_at end where id=$1`,[matchId,assemblyState]);

      await client.query('commit');
      return reply.code(201).send({
        allocationId,matchId,serverId,region:parsed.data.region,tickRate:60,connectToken,
        connectHost:connectTarget.host,connectPort:connectTarget.port,expiresAt:new Date(expiresAt).toISOString(),
        networkBuild:session.build,backendProtocol:BACKEND_PROTOCOL_VERSION,allocator,
        matchAssembly:{state:assemblyState,targetPlayers,reservedPlayers}
      });
    }catch(error){await client.query('rollback');throw error;}finally{client.release();}
  }

  const matchId=crypto.randomUUID();
  const connectTarget=requireConnectTarget(reply);if(!connectTarget)return;
  const serverId=`${parsed.data.region.toUpperCase()}-DEV-${randomBytes(2).toString('hex').toUpperCase()}`;
  return reply.code(201).send({
    allocationId,matchId,serverId,region:parsed.data.region,tickRate:60,connectToken,
    connectHost:connectTarget.host,connectPort:connectTarget.port,expiresAt:new Date(expiresAt).toISOString(),
    networkBuild:session.build,backendProtocol:BACKEND_PROTOCOL_VERSION,allocator:'static-dev',
    matchAssembly:{state:'ready',targetPlayers:1,reservedPlayers:1}
  });
});


const admissionSchema=z.object({allocationId:z.string().uuid(),matchId:z.string().uuid(),connectToken:z.string().min(24).max(256),reconnectGrantId:z.string().uuid().optional(),roundNumber:z.number().int().min(1).max(9).optional()});
app.post('/v1/matches/admit',async(req,reply)=>{
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=admissionSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(!pool)return reply.code(503).send({error:'database-not-configured'});
  if(parsed.data.reconnectGrantId){
    const admitted=await admitReconnect(pool,parsed.data,String(req.headers['x-sp-server-id']??''));
    if(!admitted)return reply.code(403).send({error:'reconnect-admission-denied'});
    return reply.send({...admitted,backendProtocol:BACKEND_PROTOCOL_VERSION});
  }
  const tokenHash=sha256(parsed.data.connectToken);
  const r=await pool.query(`update server_allocations sa
    set status='live',started_at=coalesce(sa.started_at,now()),connect_token_consumed_at=now()
    from matches m
    where sa.id=$1 and sa.match_id=$2 and m.id=sa.match_id and sa.connect_token_hash=$3
      and sa.connect_token_consumed_at is null and sa.expires_at>now() and sa.status in ('reserved','starting','ready')
    returning sa.id as allocation_id,sa.match_id,sa.server_id,sa.region,sa.user_id,m.server_build`,[parsed.data.allocationId,parsed.data.matchId,tokenHash]);
  if(!r.rowCount)return reply.code(403).send({error:'admission-denied'});
  const admitted=r.rows[0];
  return reply.send({admitted:true,allocationId:admitted.allocation_id,matchId:admitted.match_id,serverId:admitted.server_id,region:admitted.region,userId:admitted.user_id,networkBuild:admitted.server_build,backendProtocol:BACKEND_PROTOCOL_VERSION,authority:'dedicated-server'});
});

const queueSchema = z.object({
  region: z.string().min(2).max(16),
  latencyMs: z.number().int().min(0).max(2000),
  partySize: z.number().int().min(1).max(5)
});
app.post('/v1/matchmaking/queue', async (req, reply) => {
  const session=requireCompatibleSession(req,reply);if(!session)return;
  const parsed=queueSchema.safeParse(req.body);
  if (!parsed.success) return reply.code(400).send({ error:parsed.error.flatten() });
  if(parsed.data.region!==session.region)return reply.code(409).send({error:'session-region-mismatch'});

  let skillRating=1000,trustScore=75;
  if(pool){
    const r=await pool.query(`select coalesce(r.skill_rating,1000) as skill_rating,u.trust_score from users u left join rankings r on r.user_id=u.id where u.id=$1 and u.status='active'`,[session.uid]);
    if(!r.rowCount)return reply.code(403).send({error:'account-not-eligible'});
    skillRating=Number(r.rows[0].skill_rating);
    trustScore=Number(r.rows[0].trust_score);
  }

  const ticket = `mm_${crypto.randomUUID()}`;
  const serverOwnedTicket={userId:session.uid,region:parsed.data.region,latencyMs:parsed.data.latencyMs,partySize:parsed.data.partySize,skillRating,trustScore,build:session.build};
  if (redis) await redis.setex(`matchmaking:${ticket}`, 90, JSON.stringify(serverOwnedTicket));
  return { ticket, status:'queued', userId:session.uid, region:parsed.data.region, factors:['skill','party-size','region','latency','recent-performance','trust'] };
});

app.get('/v1/profiles/:userId', async (req, reply) => {
  if (!pool) return reply.code(503).send({ error:'database-not-configured' });
  const { userId } = req.params as { userId:string };
  const r = await pool.query('select p.*, u.trust_score from profiles p join users u on u.id=p.user_id where p.user_id=$1', [userId]);
  if (!r.rowCount) return reply.code(404).send({ error:'not-found' });
  return r.rows[0];
});

const taskForceSchema = z.object({ ownerUserId:z.string().uuid(), name:z.string().min(3).max(48), tag:z.string().min(2).max(6), emblemKey:z.string().max(160).optional() });
app.post('/v1/task-forces', async (req, reply) => {
  const p = taskForceSchema.safeParse(req.body);
  if (!p.success) return reply.code(400).send({ error:p.error.flatten() });
  if (!pool) return reply.code(503).send({ error:'database-not-configured' });
  const c = await pool.connect();
  try {
    await c.query('begin');
    const tf = await c.query('insert into task_forces(name,tag,emblem_key,commander_user_id) values($1,$2,$3,$4) returning *', [p.data.name,p.data.tag,p.data.emblemKey??null,p.data.ownerUserId]);
    await c.query("insert into task_force_members(task_force_id,user_id,role) values($1,$2,'commander')", [tf.rows[0].id,p.data.ownerUserId]);
    await c.query('commit'); return reply.code(201).send(tf.rows[0]);
  } catch (e) { await c.query('rollback'); throw e; } finally { c.release(); }
});

app.get('/v1/seasons/current', async (_req, reply) => {
  if (!pool) return reply.send({ code:'BLACK_TIDE', name:'BLACK TIDE', framework:true, rewards:['uniforms','weapon-skins','patches','animations','banners','profile-cosmetics'] });
  const r = await pool.query("select * from seasons where starts_at <= now() and ends_at > now() order by starts_at desc limit 1");
  return r.rows[0] ?? null;
});

app.post('/v1/anti-cheat/events', async (req, reply) => {
  if(!(await requireMatchServer(req,reply)))return;
  const s = z.object({ matchId:z.string().uuid(), userId:z.string().uuid(), signal:z.string().min(2).max(64), severity:z.number().int().min(1).max(5), evidence:z.record(z.unknown()).default({}) }).safeParse(req.body);
  if (!s.success) return reply.code(400).send({ error:s.error.flatten() });
  if (pool) await pool.query('insert into anti_cheat_events(match_id,user_id,signal,severity,evidence) values($1,$2,$3,$4,$5)', [s.data.matchId,s.data.userId,s.data.signal,s.data.severity,s.data.evidence]);
  return reply.code(202).send({ accepted:true, authority:'dedicated-server' });
});

const matchEventSchema = z.object({
  matchId:z.string().uuid(), userId:z.string().uuid().nullable().optional(), eventType:z.string().min(2).max(64),
  gameTimeMs:z.number().int().min(0), position:z.object({x:z.number(),y:z.number(),z:z.number()}).optional(), payload:z.record(z.unknown()).default({})
});
app.post('/v1/matches/events', async (req, reply) => {
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=matchEventSchema.safeParse(req.body);
  if(!parsed.success) return reply.code(400).send({error:parsed.error.flatten()});
  if(pool) await pool.query('insert into match_events(match_id,user_id,event_type,game_time_ms,position,payload) values($1,$2,$3,$4,$5,$6)', [parsed.data.matchId,parsed.data.userId??null,parsed.data.eventType,parsed.data.gameTimeMs,parsed.data.position??null,parsed.data.payload]);
  return reply.code(202).send({accepted:true,authority:'dedicated-server'});
});

const roundResultSchema = z.object({
  matchId:z.string().uuid(), roundNumber:z.number().int().min(1).max(99), attackingTeam:z.string().min(2).max(32),
  defendingTeam:z.string().min(2).max(32), winnerTeam:z.string().min(2).max(32), outcomeReason:z.string().min(2).max(160),
  startedAt:z.string().datetime().optional(), endedAt:z.string().datetime().optional(), objectiveSiteCode:z.string().max(64).nullable().optional(), objectiveState:z.record(z.unknown()).default({})
});
app.post('/v1/matches/rounds', async (req, reply) => {
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=roundResultSchema.safeParse(req.body);
  if(!parsed.success) return reply.code(400).send({error:parsed.error.flatten()});
  if(pool) await pool.query(`insert into match_rounds(match_id,round_number,attacking_team,defending_team,winner_team,outcome_reason,started_at,ended_at,objective_site_code,objective_state)
    values($1,$2,$3,$4,$5,$6,$7,$8,$9,$10)
    on conflict(match_id,round_number) do update set winner_team=excluded.winner_team,outcome_reason=excluded.outcome_reason,ended_at=excluded.ended_at,objective_site_code=excluded.objective_site_code,objective_state=excluded.objective_state`,
    [parsed.data.matchId,parsed.data.roundNumber,parsed.data.attackingTeam,parsed.data.defendingTeam,parsed.data.winnerTeam,parsed.data.outcomeReason,parsed.data.startedAt??null,parsed.data.endedAt??null,parsed.data.objectiveSiteCode??null,parsed.data.objectiveState]);
  return reply.code(202).send({accepted:true,authority:'dedicated-server'});
});

const equipmentEventSchema=z.object({
  matchId:z.string().uuid(),roundNumber:z.number().int().min(1),userId:z.string().uuid().nullable().optional(),
  equipmentType:z.enum(['flash','smoke']),position:z.object({x:z.number(),y:z.number(),z:z.number()}).optional(),
  affectedEntities:z.array(z.string()).default([]),gameTimeMs:z.number().int().min(0)
});
app.post('/v1/matches/equipment-events', async (req, reply) => {
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=equipmentEventSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(pool) await pool.query('insert into tactical_equipment_events(match_id,round_number,user_id,equipment_type,position,affected_entities,game_time_ms) values($1,$2,$3,$4,$5,$6,$7)',
    [parsed.data.matchId,parsed.data.roundNumber,parsed.data.userId??null,parsed.data.equipmentType,parsed.data.position??null,parsed.data.affectedEntities,parsed.data.gameTimeMs]);
  return reply.code(202).send({accepted:true,authority:'dedicated-server'});
});

const playerSlotSchema=z.object({
  matchId:z.string().uuid(),roundNumber:z.number().int().min(1).max(9),slotIndex:z.number().int().min(0).max(9),
  userId:z.string().uuid().nullable().optional(),team:z.string().min(2).max(32),tacticalSide:z.enum(['attack','defense']),
  spawnGroup:z.string().max(64).nullable().optional(),connectionState:z.enum(['connected','reconnecting','disconnected']).default('connected'),ready:z.boolean().default(false),reconnectTokenHash:z.string().max(256).nullable().optional(),reconnectDeadline:z.string().datetime().nullable().optional()
});
app.post('/v1/matches/player-slots', async (req,reply)=>{
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=playerSlotSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(pool)await pool.query(`insert into match_player_slots(match_id,round_number,slot_index,user_id,team,tactical_side,spawn_group,connection_state,ready,reconnect_token_hash,reconnect_deadline)
    values($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11) on conflict(match_id,round_number,slot_index) do update set user_id=excluded.user_id,team=excluded.team,tactical_side=excluded.tactical_side,spawn_group=excluded.spawn_group,connection_state=excluded.connection_state,ready=excluded.ready,reconnect_token_hash=excluded.reconnect_token_hash,reconnect_deadline=excluded.reconnect_deadline`,
    [parsed.data.matchId,parsed.data.roundNumber,parsed.data.slotIndex,parsed.data.userId??null,parsed.data.team,parsed.data.tacticalSide,parsed.data.spawnGroup??null,parsed.data.connectionState,parsed.data.ready,parsed.data.reconnectTokenHash??null,parsed.data.reconnectDeadline??null]);
  return reply.code(202).send({accepted:true,authority:'dedicated-server'});
});

const fortificationEventSchema=z.object({
  matchId:z.string().uuid(),roundNumber:z.number().int().min(1).max(9),userId:z.string().uuid().nullable().optional(),
  action:z.enum(['placed','damaged','destroyed']),fortificationType:z.string().min(2).max(48).default('barricade'),
  position:z.object({x:z.number(),y:z.number(),z:z.number()}),healthRemaining:z.number().min(0).max(10000).nullable().optional(),gameTimeMs:z.number().int().min(0)
});
app.post('/v1/matches/fortification-events',async(req,reply)=>{
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=fortificationEventSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(pool)await pool.query('insert into fortification_events(match_id,round_number,user_id,action,fortification_type,position,health_remaining,game_time_ms) values($1,$2,$3,$4,$5,$6,$7,$8)',
    [parsed.data.matchId,parsed.data.roundNumber,parsed.data.userId??null,parsed.data.action,parsed.data.fortificationType,parsed.data.position,parsed.data.healthRemaining??null,parsed.data.gameTimeMs]);
  return reply.code(202).send({accepted:true,authority:'dedicated-server'});
});

const ballisticEventSchema=z.object({
  matchId:z.string().uuid(),roundNumber:z.number().int().min(1).max(9),shooterUserId:z.string().uuid().nullable().optional(),
  eventType:z.enum(['penetration','suppression']),material:z.string().max(64).nullable().optional(),targetUserId:z.string().uuid().nullable().optional(),
  position:z.object({x:z.number(),y:z.number(),z:z.number()}).nullable().optional(),payload:z.record(z.unknown()).default({}),gameTimeMs:z.number().int().min(0)
});
app.post('/v1/matches/ballistic-events',async(req,reply)=>{
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=ballisticEventSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(pool)await pool.query('insert into ballistic_events(match_id,round_number,shooter_user_id,event_type,material,target_user_id,position,payload,game_time_ms) values($1,$2,$3,$4,$5,$6,$7,$8,$9)',
    [parsed.data.matchId,parsed.data.roundNumber,parsed.data.shooterUserId??null,parsed.data.eventType,parsed.data.material??null,parsed.data.targetUserId??null,parsed.data.position??null,parsed.data.payload,parsed.data.gameTimeMs]);
  return reply.code(202).send({accepted:true,authority:'dedicated-server'});
});

const readyStateSchema=z.object({
  matchId:z.string().uuid(),roundNumber:z.number().int().min(1).max(9),slotIndex:z.number().int().min(0).max(9),ready:z.boolean(),spawnGroup:z.string().min(1).max(64).optional()
});
app.post('/v1/matches/ready-state',async(req,reply)=>{
  const session=requireCompatibleSession(req,reply);if(!session)return;
  const parsed=readyStateSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(!pool)return reply.code(503).send({error:'database-not-configured'});
  const r=await pool.query(`update match_player_slots set ready=$4,spawn_group=coalesce($5,spawn_group) where match_id=$1 and round_number=$2 and slot_index=$3 and user_id=$6 returning slot_index,ready,spawn_group,connection_state`,
    [parsed.data.matchId,parsed.data.roundNumber,parsed.data.slotIndex,parsed.data.ready,parsed.data.spawnGroup??null,session.uid]);
  if(!r.rowCount)return reply.code(403).send({error:'slot-ownership-required'});
  return reply.send(r.rows[0]);
});

const reconnectTicketSchema=z.object({matchId:z.string().uuid(),roundNumber:z.number().int().min(1).max(9),slotIndex:z.number().int().min(0).max(9)});
app.post('/v1/matches/reconnect-ticket',async(req,reply)=>{
  const session=requireCompatibleSession(req,reply);if(!session)return;
  const parsed=reconnectTicketSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(!pool)return reply.code(503).send({error:'database-not-configured'});
  const token=randomBytes(32).toString('base64url'),hash=sha256(token);
  // A reservation is created once from a connected slot. Repeated requests must
  // not replace the token or extend its deadline, even under concurrent retries.
  const r=await pool.query(`update match_player_slots s
    set connection_state='reconnecting',ready=false,reconnect_token_hash=$4,reconnect_deadline=now()+interval '90 seconds'
    from matches m
    where s.match_id=$1 and s.round_number=$2 and s.slot_index=$3 and s.user_id=$5
      and s.connection_state='connected' and s.reconnect_token_hash is null and s.reconnect_deadline is null
      and m.id=s.match_id and m.ended_at is null and m.region=$6 and m.server_build=$7
      and exists(select 1 from server_allocations a where a.match_id=m.id and a.status='live')
    returning s.slot_index,s.reconnect_deadline`,[parsed.data.matchId,parsed.data.roundNumber,parsed.data.slotIndex,hash,session.uid,session.region,session.build]);
  if(!r.rowCount)return reply.code(403).send({error:'reconnect-reservation-denied'});
  return reply.code(201).send({reconnectToken:token,reconnectDeadline:new Date(r.rows[0].reconnect_deadline).toISOString(),graceSeconds:90,networkBuild:session.build,backendProtocol:BACKEND_PROTOCOL_VERSION});
});

const reconnectSchema=z.object({matchId:z.string().uuid(),roundNumber:z.number().int().min(1).max(9),slotIndex:z.number().int().min(0).max(9),reconnectToken:z.string().min(32).max(256)});
app.post('/v1/matches/reconnect-allocation',async(req,reply)=>{
  const session=requireCompatibleSession(req,reply);if(!session)return;
  const parsed=reconnectSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(!pool)return reply.code(503).send({error:'database-not-configured'});
  const allocation=await exchangeReconnect(pool,parsed.data,session);
  if(!allocation)return reply.code(403).send({error:'reconnect-allocation-denied'});
  return reply.code(201).send({...allocation,backendProtocol:BACKEND_PROTOCOL_VERSION});
});

app.post('/v1/matches/reconnect',async(req,reply)=>{
  const session=requireCompatibleSession(req,reply);if(!session)return;
  const parsed=reconnectSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(!pool)return reply.code(503).send({error:'database-not-configured'});
  const tokenHash=sha256(parsed.data.reconnectToken);
  const r=await pool.query(`update match_player_slots s
    set connection_state='connected',ready=false,reconnect_deadline=null,reconnect_token_hash=null
    from matches m
    where s.match_id=$1 and s.round_number=$2 and s.slot_index=$3 and s.user_id=$4
      and s.connection_state='reconnecting' and s.reconnect_token_hash=$5 and s.reconnect_deadline>now()
      and m.id=s.match_id and m.ended_at is null and m.region=$6 and m.server_build=$7
      and exists(select 1 from server_allocations a where a.match_id=m.id and a.status='live')
    returning s.slot_index,s.user_id,s.team,s.spawn_group`,[parsed.data.matchId,parsed.data.roundNumber,parsed.data.slotIndex,session.uid,tokenHash,session.region,session.build]);
  if(!r.rowCount)return reply.code(403).send({error:'reconnect-denied'});
  return reply.send({reconnected:true,slot:r.rows[0],networkBuild:session.build,backendProtocol:BACKEND_PROTOCOL_VERSION});
});

const killFeedSchema=z.object({
  matchId:z.string().uuid(),roundNumber:z.number().int().min(1).max(9),killerUserId:z.string().uuid().nullable().optional(),victimUserId:z.string().uuid().nullable().optional(),killerTeam:z.string().min(2).max(32),victimTeam:z.string().min(2).max(32),weaponCode:z.string().max(64).nullable().optional(),headshot:z.boolean().default(false),gameTimeMs:z.number().int().min(0)
});
app.post('/v1/matches/kill-feed',async(req,reply)=>{
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=killFeedSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(pool)await pool.query('insert into kill_feed_events(match_id,round_number,killer_user_id,victim_user_id,killer_team,victim_team,weapon_code,headshot,game_time_ms) values($1,$2,$3,$4,$5,$6,$7,$8,$9)',
    [parsed.data.matchId,parsed.data.roundNumber,parsed.data.killerUserId??null,parsed.data.victimUserId??null,parsed.data.killerTeam,parsed.data.victimTeam,parsed.data.weaponCode??null,parsed.data.headshot,parsed.data.gameTimeMs]);
  return reply.code(202).send({accepted:true,authority:'dedicated-server'});
});

const overtimeSchema=z.object({matchId:z.string().uuid(),roundNumber:z.number().int().min(1).max(9),triggerReason:z.string().min(2).max(160),durationSeconds:z.number().int().min(1).max(120).default(30)});
app.post('/v1/matches/overtime',async(req,reply)=>{
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=overtimeSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(pool)await pool.query('insert into match_overtime_events(match_id,round_number,trigger_reason,duration_seconds) values($1,$2,$3,$4)',[parsed.data.matchId,parsed.data.roundNumber,parsed.data.triggerReason,parsed.data.durationSeconds]);
  return reply.code(202).send({accepted:true,authority:'dedicated-server'});
});

const environmentEventSchema=z.object({
  matchId:z.string().uuid(),roundNumber:z.number().int().min(1).max(9),userId:z.string().uuid().nullable().optional(),
  eventType:z.enum(['sector_enter','footstep','breach_fx','suppression_fx','weapon_state']),sectorCode:z.string().max(64).nullable().optional(),
  surfaceProfile:z.string().max(32).nullable().optional(),position:z.record(z.any()).nullable().optional(),payload:z.record(z.any()).default({}),gameTimeMs:z.number().int().min(0)
});
app.post('/v1/matches/environment-events',async(req,reply)=>{
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=environmentEventSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(pool)await pool.query('insert into environment_events(match_id,round_number,user_id,event_type,sector_code,surface_profile,position,payload,game_time_ms) values($1,$2,$3,$4,$5,$6,$7,$8,$9)',[parsed.data.matchId,parsed.data.roundNumber,parsed.data.userId??null,parsed.data.eventType,parsed.data.sectorCode??null,parsed.data.surfaceProfile??null,parsed.data.position??null,parsed.data.payload,parsed.data.gameTimeMs]);
  return reply.code(202).send({accepted:true,authority:'dedicated-server'});
});

const combatEventSchema=z.object({
  matchId:z.string().uuid(),roundNumber:z.number().int().min(1).max(9),shooterUserId:z.string().uuid(),victimUserId:z.string().uuid(),
  eventType:z.enum(['damage','elimination','assist','team_damage','headshot']),weaponCode:z.string().min(2).max(64),damage:z.number().min(0).max(1000),
  bodyZone:z.enum(['head','torso','left_arm','right_arm','left_leg','right_leg']).default('torso'),clientShotAgeMs:z.number().min(0).max(500),
  serverShotTimeMs:z.number().int().min(0),friendlyFire:z.boolean().default(false),payload:z.record(z.unknown()).default({})
});
app.post('/v1/matches/combat-events',async(req,reply)=>{
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=combatEventSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(pool)await pool.query(`insert into combat_events(match_id,round_number,shooter_user_id,victim_user_id,event_type,weapon_code,damage,body_zone,client_shot_age_ms,server_shot_time_ms,friendly_fire,payload) values($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12)`,[parsed.data.matchId,parsed.data.roundNumber,parsed.data.shooterUserId,parsed.data.victimUserId,parsed.data.eventType,parsed.data.weaponCode,parsed.data.damage,parsed.data.bodyZone,parsed.data.clientShotAgeMs,parsed.data.serverShotTimeMs,parsed.data.friendlyFire,parsed.data.payload]);
  return reply.code(202).send({accepted:true,authority:'dedicated-server'});
});

app.get('/v1/live', { websocket:true }, (socket) => {
  socket.send(JSON.stringify({ type:'hello', system:'SHADOW PROTOCOL', message:'EVERY MOVE IS CLASSIFIED.', networkBuild:NETWORK_BUILD, backendProtocol:BACKEND_PROTOCOL_VERSION }));
  socket.on('message', (raw:any) => {
    // Production socket accepts authenticated presence/party events only; authoritative match state stays on dedicated server.
    socket.send(JSON.stringify({ type:'ack', receivedBytes:raw.byteLength }));
  });
});

const port = Number(process.env.PORT ?? 8080);

const tacticalInteractionSchema=z.object({
  matchId:z.string().uuid(),roundNumber:z.number().int().min(1).max(9),userId:z.string().uuid().optional(),
  eventType:z.enum(['door_peek','door_open','door_breach','camera_destroyed','light_destroyed','vault','lean','optic_change','vertical_route']),
  objectCode:z.string().max(80).optional(),sectorCode:z.string().max(80).optional(),position:z.record(z.any()).optional(),payload:z.record(z.any()).default({}),gameTimeMs:z.number().int().min(0)
});
app.post('/v1/matches/tactical-interactions',async(req,reply)=>{
  if(!(await requireMatchServer(req,reply)))return;
  const parsed=tacticalInteractionSchema.safeParse(req.body);if(!parsed.success)return reply.code(400).send({error:parsed.error.flatten()});
  if(pool)await pool.query('insert into tactical_interaction_events(match_id,round_number,user_id,event_type,object_code,sector_code,position,payload,game_time_ms) values($1,$2,$3,$4,$5,$6,$7,$8,$9)',[parsed.data.matchId,parsed.data.roundNumber,parsed.data.userId??null,parsed.data.eventType,parsed.data.objectCode??null,parsed.data.sectorCode??null,parsed.data.position??null,parsed.data.payload,parsed.data.gameTimeMs]);
  return reply.code(202).send({accepted:true,authority:'dedicated-server'});
});

await app.listen({ port, host:'0.0.0.0' });