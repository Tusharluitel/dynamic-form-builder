# Apache Solr Integration — Step-by-Step Guide

Full-text search for the Dynamic Form Builder using Apache Solr 9, the Drupal 7
`apachesolr` module (already in `sites/all/modules/apachesolr-7.x-1.x`), and
Facet API (already in `sites/all/modules/facetapi-7.x-1.x`).

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Install Apache Solr 9](#2-install-apache-solr-9)
3. [Create a Drupal Core](#3-create-a-drupal-core)
4. [Copy the Drupal Config Files](#4-copy-the-drupal-config-files)
5. [Start / Restart the Core](#5-start--restart-the-core)
6. [Enable the Drupal Modules](#6-enable-the-drupal-modules)
7. [Configure the Solr Connection](#7-configure-the-solr-connection)
8. [Index Content](#8-index-content)
9. [Set Up Faceted Search](#9-set-up-faceted-search)
10. [Verify Everything Works](#10-verify-everything-works)
11. [Troubleshooting](#11-troubleshooting)
12. [Managing Solr](#12-managing-solr)

---

## 1. Prerequisites

| Requirement   | Your System          | Status |
|---------------|----------------------|--------|
| Java 17+      | OpenJDK 17.0.18      | ✅     |
| Drupal 7      | Running on XAMPP      | ✅     |
| apachesolr module | `apachesolr-7.x-1.x` | ✅ (already downloaded) |
| facetapi module   | `facetapi-7.x-1.x`   | ✅ (already downloaded) |

Nothing else to install for prerequisites.

---

## 2. Install Apache Solr 9

> **Already done.** Solr 9.10.1 downloaded and extracted to:
> `/Users/tusharluitel/Desktop/ITONICS/solr-9.10.1`

Verify:

```bash
/Users/tusharluitel/Desktop/ITONICS/solr-9.10.1/bin/solr version
```

Expected output: `9.x.x` (e.g., `9.7.0`).

### Solr Installation Location

> **Already done.** Solr 9.10.1 is installed manually at:
> `/Users/tusharluitel/Desktop/ITONICS/solr-9.10.1`

### Start / Stop Solr

```bash
cd /Users/tusharluitel/Desktop/ITONICS/solr-9.10.1
bin/solr start          # Start in background
bin/solr stop -all      # Stop
bin/solr restart        # Restart
```

Verify it's running by visiting:

> **http://localhost:8983/solr/**

You should see the Solr Admin UI.

---

## 3. Create a Solr Core

> **Already done.** Your core `dynamic-form-builder` is live at:
> http://localhost:8983/solr/#/~cores/dynamic-form-builder

Core config directory:

```
/Users/tusharluitel/Desktop/ITONICS/solr-9.10.1/server/solr/dynamic-form-builder/conf/
```

---

## 4. Copy the Drupal Config Files

> **Already done.** The Drupal `solr-conf/solr-9.x` files are already in
> your core's `conf/` directory.

For reference, here are the commands that were used (no need to re-run):

```bash
DRUPAL_ROOT="/Applications/XAMPP/xamppfiles/htdocs/dynamic-form-builder"
SOLR_CONF="$DRUPAL_ROOT/sites/all/modules/apachesolr-7.x-1.x/solr-conf/solr-9.x"
CORE_CONF="/Users/tusharluitel/Desktop/ITONICS/solr-9.10.1/server/solr/dynamic-form-builder/conf"

cp "$SOLR_CONF/schema.xml"                   "$CORE_CONF/"
cp "$SOLR_CONF/solrconfig.xml"               "$CORE_CONF/"
cp "$SOLR_CONF/solrconfig_extra.xml"         "$CORE_CONF/"
cp "$SOLR_CONF/schema_extra_fields.xml"      "$CORE_CONF/"
cp "$SOLR_CONF/schema_extra_types.xml"       "$CORE_CONF/"
cp "$SOLR_CONF/elevate.xml"                  "$CORE_CONF/"
cp "$SOLR_CONF/mapping-ISOLatin1Accent.txt"  "$CORE_CONF/"
cp "$SOLR_CONF/protwords.txt"                "$CORE_CONF/"
cp "$SOLR_CONF/stopwords.txt"                "$CORE_CONF/"
cp "$SOLR_CONF/synonyms.txt"                 "$CORE_CONF/"
cp "$SOLR_CONF/solrcore.properties"          "$CORE_CONF/"
```

### Important: Remove the managed-schema file

Solr 9 uses `managed-schema` by default, but the Drupal module uses the
classic `schema.xml` approach. If `managed-schema` exists, remove it:

```bash
rm "/Users/tusharluitel/Desktop/ITONICS/solr-9.10.1/server/solr/dynamic-form-builder/conf/managed-schema" 2>/dev/null
```

Then ensure `solrconfig.xml` uses the classic schema factory. Open the copied
`solrconfig.xml` and verify it contains:

```xml
<schemaFactory class="ClassicIndexSchemaFactory"/>
```

The Drupal-provided `solrconfig.xml` should already have this. If not, add it
inside the `<config>` block.

---

## 5. Start / Restart the Core

Reload the core so Solr picks up the new config files:

```bash
# If Solr is already running:
solr restart

# Or reload just the core via the API:
curl "http://localhost:8983/solr/admin/cores?action=RELOAD&core=dynamic-form-builder"
```

### Verify the core is healthy

Visit: **http://localhost:8983/solr/#/dynamic-form-builder**

You should see the `dynamic-form-builder` core in the dropdown with **no errors**. The
"Num Docs" will be 0 (empty index).

If the core fails to load, check the Solr log:

```bash
tail -100 /Users/tusharluitel/Desktop/ITONICS/solr-9.10.1/server/logs/solr.log
```

---

## 6. Enable the Drupal Modules

Navigate to your Drupal admin:

> **http://localhost/dynamic-form-builder/?q=admin/modules**

Enable these modules in order:

1. **Search** (core module — may already be enabled)
2. **Apache Solr framework** (`apachesolr`)
3. **Apache Solr search** (`apachesolr_search`)
4. **Facet API** (`facetapi`) — for faceted search

Save the module configuration.

---

## 7. Configure the Solr Connection

### 7a. Set the Server URL

Navigate to:

> **http://localhost/dynamic-form-builder/?q=admin/config/search/apachesolr/settings**

Click **Edit** on the default environment (or add a new one). Set:

| Field       | Value                              |
|-------------|------------------------------------|
| Name        | `Local Solr 9`                     |
| URL         | `http://localhost:8983/solr/dynamic-form-builder` |

Click **Save**. The status should show a **green checkmark** indicating a
successful connection.

### 7b. Make Solr the Default Search

Navigate to:

> **http://localhost/dynamic-form-builder/?q=admin/config/search/settings**

Under "Default search module", select **Apache Solr search**. Save.

### 7c. Configure What Gets Indexed

Navigate to:

> **http://localhost/dynamic-form-builder/?q=admin/config/search/apachesolr/settings/solr/index**

Configure:

| Setting                  | Recommended Value |
|--------------------------|-------------------|
| Items to index per cron  | 50                |
| Node types to index      | Select all (or just the types you want searchable) |

---

## 8. Index Content

### Option A: Run Cron

Drupal indexes content in batches during cron runs:

```bash
# Via Drush (if installed):
drush cron

# Or visit in the browser:
# http://localhost/dynamic-form-builder/cron.php
```

### Option B: Trigger Indexing Manually

Navigate to:

> **http://localhost/dynamic-form-builder/?q=admin/config/search/apachesolr/settings/solr/index**

Click **"Index queued content"** to immediately push all queued items to Solr.

### Monitor Progress

On the same index page, you'll see:

- **Total documents in index**: How many items Solr has
- **Remaining to index**: How many are queued

Keep running cron until "Remaining" reaches 0.

### Verify in Solr Admin

Visit: **http://localhost:8983/solr/#/dynamic-form-builder/query**

Run a query with `*:*` to see all indexed documents.

---

## 9. Set Up Faceted Search

### 9a. Enable Facets

Navigate to:

> **http://localhost/dynamic-form-builder/?q=admin/config/search/apachesolr/settings/solr/facets**

Enable the facets you want. Common choices:

| Facet                | Description                    |
|----------------------|--------------------------------|
| Content type         | Filter by node type            |
| Author               | Filter by content author       |
| Date (created)       | Filter by creation date        |
| Taxonomy terms       | Filter by tags/categories      |

### 9b. Place Facet Blocks

Navigate to:

> **http://localhost/dynamic-form-builder/?q=admin/structure/block**

Find the Facet API blocks (they appear under the "Apache Solr" region).
Place them in your desired sidebar region.

### 9c. Test Faceted Search

Visit the search page and perform a search. The facet blocks should appear
in the sidebar, letting users narrow results by content type, author, date,
etc.

---

## 10. Verify Everything Works

### Checklist

| Check | How |
|-------|-----|
| ✅ Solr is running | Visit http://localhost:8983/solr/ |
| ✅ Drupal connects | Admin → Apache Solr → Settings → green checkmark |
| ✅ Content is indexed | Admin → Apache Solr → Index → "0 remaining" |
| ✅ Search works | Visit the search page, enter a query, get results |
| ✅ Facets display | Facet blocks appear in the sidebar during search |

### Quick Search Test

Navigate to:

> **http://localhost/dynamic-form-builder/?q=search/site/test**

Replace `test` with any term that appears in your content. You should see
Solr-powered search results.

---

## 11. Troubleshooting

### "No Solr instance available"

- Verify Solr is running: `solr status`
- Check the URL in Drupal matches exactly (including the core name):
  `http://localhost:8983/solr/dynamic-form-builder`

### Core fails to load after copying configs

- Check the Solr log: `tail -100 /Users/tusharluitel/Desktop/ITONICS/solr-9.10.1/server/logs/solr.log`
- Most common cause: `managed-schema` still exists alongside `schema.xml`.
  Remove it: `rm <core-conf>/managed-schema`
- Ensure `solrconfig.xml` has `<schemaFactory class="ClassicIndexSchemaFactory"/>`

### Content isn't appearing in search

- Run cron: `drush cron` or visit `cron.php`
- Check the index page: `?q=admin/config/search/apachesolr/settings/solr/index`
- Re-index: click "Delete the index and re-index" if needed
- Solr auto-commit can delay visibility by 1–2 minutes

### "HTTP 404" when connecting

- The core name is case-sensitive. Ensure it's `dynamic-form-builder` in both
  the Solr core and the Drupal URL setting.

### "java.lang.OutOfMemoryError"

- Increase Solr's heap:
  ```bash
  solr stop
  SOLR_JAVA_MEM="-Xms512m -Xmx1g" solr start
  ```

---

## 12. Managing Solr

### Start / Stop / Restart

```bash
solr start          # Start in background
solr stop -all      # Stop all instances
solr restart        # Restart
solr status         # Check status
```

### Auto-start on Login (macOS)

Create a macOS launch agent, or simply start Solr manually before development:

```bash
cd /Users/tusharluitel/Desktop/ITONICS/solr-9.10.1
bin/solr start
```

### Re-index Everything

From Drupal admin:

> Admin → Config → Search → Apache Solr → Index → **"Delete the index and
> re-index all content"**

Then run cron repeatedly until indexing completes.

### Delete the Core

```bash
solr delete -c dynamic-form-builder
```

### Solr Admin UI Quick Links

| Page                  | URL                                      |
|-----------------------|------------------------------------------|
| Dashboard             | http://localhost:8983/solr/                                |
| Core overview         | http://localhost:8983/solr/#/dynamic-form-builder           |
| Query interface       | http://localhost:8983/solr/#/dynamic-form-builder/query     |
| Schema browser        | http://localhost:8983/solr/#/dynamic-form-builder/schema    |
| Logging               | http://localhost:8983/solr/#/~logging                       |

---

## Architecture Overview

```
┌────────────────────────┐
│  User's Browser        │
│  Search query          │
└──────────┬─────────────┘
           │ HTTP
           ▼
┌────────────────────────┐
│  Drupal 7 (XAMPP)      │
│  apachesolr module     │──── reads/writes ──── MySQL (forms, nodes)
│  facetapi module       │
└──────────┬─────────────┘
           │ HTTP (port 8983)
           ▼
┌────────────────────────┐
│  Apache Solr 9         │
│  Core: "drupal"        │
│  Lucene index          │
└────────────────────────┘
```

- **Drupal** sends content to Solr during cron (indexing)
- **Drupal** queries Solr when a user searches (querying)
- **Solr** returns matching document IDs, scores, and facet counts
- **Drupal** loads the full node data from MySQL and renders results

---

## Files Reference

| File | Location | Purpose |
|------|----------|---------|
| `schema.xml` | Solr core `conf/` | Defines fields, types, and analyzers for Drupal content |
| `solrconfig.xml` | Solr core `conf/` | Request handlers, auto-commit, caching |
| `protwords.txt` | Solr core `conf/` | Words excluded from stemming |
| `stopwords.txt` | Solr core `conf/` | Common words excluded from indexing |
| `synonyms.txt` | Solr core `conf/` | Synonym mappings for search |
| `elevate.xml` | Solr core `conf/` | Manually boosted/excluded documents |
