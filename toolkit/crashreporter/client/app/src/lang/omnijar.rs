/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::language_info::LanguageInfo;
use super::zip::{read_archive_file_as_string, read_zip};
use crate::config::installation_resource_path;
use crate::std::io::{BufRead, BufReader};
use anyhow::Context;

/// Read the appropriate localization fluent definitions from the omnijar.
///
/// Returns language information if found in the adjacent omnijar.
pub fn read() -> anyhow::Result<Vec<LanguageInfo>> {
    let path = installation_resource_path().join("omni.ja");

    let mut zip = read_zip(&path)?;
    let buf = BufReader::new(
        zip.by_name("res/multilocale.txt")
            .context("failed to read multilocale file in zip archive")?,
    );
    let line = buf
        .lines()
        .next()
        .ok_or(anyhow::anyhow!("multilocale file was empty"))?
        .context("failed to read first line of multilocale file")?;
    let locales = line.split(",").map(|s| s.trim());

    let loaded: Vec<_> = locales
        .clone()
        .filter_map(|locale| {
            match read_archive_file_as_string(
                &mut zip,
                &format!("localization/{locale}/crashreporter/crashreporter.ftl"),
            ) {
                Ok(v) => Some((locale.to_owned(), v)),
                Err(e) => {
                    log::warn!("failed to get localized strings for {locale}: {e:#}");
                    None
                }
            }
        })
        .collect();

    anyhow::ensure!(
        !loaded.is_empty(),
        "failed to find any usable localized strings in the omnijar"
    );

    // Only use the first branding file we find (rather than loading from all locales). The
    // branding file is relatively static so we shouldn't encounter incompatibilities (e.g. missing
    // identifiers).
    let ftl_branding = 'branding: {
        for locale in locales {
            let brand_file = format!("localization/{locale}/branding/brand.ftl");
            // Bug 1895244: Thunderbird branding is in the main omnijar
            let result = read_archive_file_as_string(&mut zip, &brand_file).or_else(|_| {
                // Firefox keeps its application resources in the browser subtree.
                read_archive_file_as_string(&mut zip, &format!("browser/{brand_file}"))
            });
            match result {
                Ok(b) => break 'branding b,
                Err(e) => log::warn!("failed to read branding for {locale} from omnijar: {e:#}"),
            }
        }
        log::info!("using fallback branding info");
        LanguageInfo::default().ftl_branding
    };

    Ok(loaded
        .into_iter()
        .map(|(identifier, ftl_definitions)| LanguageInfo {
            identifier,
            ftl_definitions,
            ftl_branding: ftl_branding.clone(),
        })
        .collect())
}
