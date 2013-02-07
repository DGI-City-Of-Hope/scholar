<br/>
<div id="ir-quick-search-form">
  <?php print drupal_get_form('scholar_search_form') ?>
</div>
<?php if ($browse_by_user || $browse_by_department): ?>
  <div>or</div>
  <br/>
  <?php if ($browse_by_user): ?>
    <h1>Browse by Name</h1> 
    <div id="ir-browse-by-user-form">
      <?php print drupal_get_form('scholar_browse_by_user_form') ?>
    </div>
    <br/>
  <?php endif; ?>
<?php endif; ?>